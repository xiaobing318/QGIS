(function () {
  'use strict';

  const VIRTUAL_ORIGIN = 'https://qcopilots.localmachine';
  const VIRTUAL_HOST = new URL(VIRTUAL_ORIGIN).hostname;
  const originalFetch = window.fetch.bind(window);

  function installBlockOnlyFetch() {
    window.__qcopilotsMcpBlockOnlyInstalled = true;
    window.fetch = function (input, init) {
      let url;
      try {
        url = requestUrl(input);
      } catch (error) {
        return Promise.reject(error);
      }
      if (targetsVirtualMcpHost(url)) {
        return rejectVirtualMcpRequest('The local MCP virtual host is unavailable in this page.');
      }
      return originalFetch(input, init);
    };
  }

  function effectiveOriginPort(url) {
    if (url.port) {
      return url.port;
    }
    return url.protocol === 'http:' ? '80' : (url.protocol === 'https:' ? '443' : '');
  }

  let configuredOrigin;
  try {
    configuredOrigin = new URL(window.__qcopilotsConfiguredOriginV1);
  } catch (_error) {
    installBlockOnlyFetch();
    return;
  }
  if (window.top !== window
      || window.location.protocol.toLowerCase() !== configuredOrigin.protocol.toLowerCase()
      || window.location.hostname.toLowerCase() !== configuredOrigin.hostname.toLowerCase()
      || effectiveOriginPort(window.location) !== effectiveOriginPort(configuredOrigin)) {
    installBlockOnlyFetch();
    return;
  }
  if (window.__qcopilotsNativeMcpInstalled) {
    return;
  }
  window.__qcopilotsNativeMcpInstalled = true;

  const CONFIG_KEY = 'LlamaUi.config';
  const MANAGED_IDS_KEY = 'QCopilots.managedMcpServerIdsV1';
  const FIRST_VISIT_RELOAD_KEY = 'QCopilots.mcpFirstVisitReloadedV1';
  const CATALOG_RELOAD_GENERATION_KEY = 'QCopilots.mcpReloadedGenerationV1';
  const AUTO_REGISTRATION_DISABLED_KEY = 'QCopilots.mcpAutoRegistrationDisabledV1';
  const VIRTUAL_PATH_PREFIX = '/mcp/';
  const CATALOG_WAIT_MS = 30000;
  const REQUEST_TIMEOUT_MS = 300000;
  const MAX_BODY_BYTES = 1024 * 1024;
  const ALLOWED_REQUEST_HEADERS = new Set([
    'accept',
    'content-type',
    'last-event-id',
    'mcp-protocol-version',
    'mcp-session-id'
  ]);

  const originalGetItem = Storage.prototype.getItem;
  const originalSetItem = Storage.prototype.setItem;
  let firstVisitAtDocumentCreation = originalGetItem.call(localStorage, CONFIG_KEY) === null;

  let firstConfigWriteObserved = false;
  let catalogWaitExpired = false;
  let catalogActivated = false;
  let currentCatalog = null;
  let nativeBridge = null;
  let requestCounter = 0;
  let autoRegistrationDisabled = false;
  const pendingRequests = new Map();

  try {
    autoRegistrationDisabled = sessionStorage.getItem(AUTO_REGISTRATION_DISABLED_KEY) === '1';
  } catch (_error) {
    autoRegistrationDisabled = true;
  }

  let resolveBridge;
  let rejectBridge;
  const bridgeReady = new Promise(function (resolve, reject) {
    resolveBridge = resolve;
    rejectBridge = reject;
  });
  bridgeReady.catch(function () {});
  window.setTimeout(function () {
    if (!nativeBridge) {
      rejectBridge(new Error('The native QCopilots MCP bridge did not become available within 30 seconds.'));
    }
  }, CATALOG_WAIT_MS);

  function exactObjectKeys(value, expected) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      return false;
    }
    const actual = Object.keys(value).sort();
    const wanted = expected.slice().sort();
    return actual.length === wanted.length && actual.every(function (key, index) {
      return key === wanted[index];
    });
  }

  function validPublicService(service) {
    if (!exactObjectKeys(service, ['id', 'displayName', 'state', 'virtualUrl'])) {
      return false;
    }
    if (typeof service.id !== 'string' || !/^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$/.test(service.id)) {
      return false;
    }
    if (typeof service.displayName !== 'string' || !service.displayName.trim()) {
      return false;
    }
    if (!['starting', 'running', 'stopped', 'failed'].includes(service.state)) {
      return false;
    }
    if (typeof service.virtualUrl !== 'string') {
      return false;
    }
    try {
      const url = new URL(service.virtualUrl);
      return url.origin === VIRTUAL_ORIGIN
        && url.username === ''
        && url.password === ''
        && url.search === ''
        && url.hash === ''
        && url.pathname.startsWith(VIRTUAL_PATH_PREFIX)
        && decodeURIComponent(url.pathname.slice(VIRTUAL_PATH_PREFIX.length)) === service.id;
    } catch (_error) {
      return false;
    }
  }

  function parsePublicCatalog(serialized) {
    if (typeof serialized !== 'string' || serialized.length > MAX_BODY_BYTES) {
      return null;
    }
    let value;
    try {
      value = JSON.parse(serialized);
    } catch (_error) {
      return null;
    }
    if (!exactObjectKeys(value, ['schemaVersion', 'generation', 'startupComplete', 'services'])
        || value.schemaVersion !== 1
        || !Number.isSafeInteger(value.generation)
        || value.generation < 0
        || typeof value.startupComplete !== 'boolean'
        || !Array.isArray(value.services)
        || value.services.length > 128) {
      return null;
    }
    const ids = new Set();
    const urls = new Set();
    for (const service of value.services) {
      if (!validPublicService(service) || ids.has(service.id) || urls.has(service.virtualUrl)) {
        return null;
      }
      ids.add(service.id);
      urls.add(service.virtualUrl);
    }
    return value;
  }

  function previousManagedIds() {
    const raw = originalGetItem.call(localStorage, MANAGED_IDS_KEY);
    if (raw === null) {
      return new Set();
    }
    try {
      const parsed = JSON.parse(raw);
      if (!Array.isArray(parsed) || parsed.some(function (id) { return typeof id !== 'string'; })) {
        return new Set();
      }
      return new Set(parsed);
    } catch (_error) {
      return new Set();
    }
  }

  function disableAutoRegistrationForSession() {
    if (autoRegistrationDisabled) {
      return;
    }
    autoRegistrationDisabled = true;
    try {
      sessionStorage.setItem(AUTO_REGISTRATION_DISABLED_KEY, '1');
    } catch (_error) {
      // The in-memory flag still protects the current page.
    }
    console.warn('[QCopilots MCP] Automatic registration is disabled for this session because the stored configuration is invalid.');
  }

  function hasManagedVirtualUrl(entry) {
    if (!entry || typeof entry !== 'object' || typeof entry.url !== 'string') {
      return false;
    }
    try {
      const url = new URL(entry.url);
      return url.origin === VIRTUAL_ORIGIN
        && url.search === ''
        && url.hash === ''
        && /^\/mcp\/[^/]+$/.test(url.pathname);
    } catch (_error) {
      return false;
    }
  }

  function mergeConfigValue(serialized, catalog) {
    let config;
    try {
      config = JSON.parse(serialized);
    } catch (_error) {
      return { ok: false, changed: false, value: serialized };
    }
    if (!config || typeof config !== 'object' || Array.isArray(config)) {
      return { ok: false, changed: false, value: serialized };
    }

    let existingServers = [];
    if (Object.prototype.hasOwnProperty.call(config, 'mcpServers')) {
      if (typeof config.mcpServers !== 'string') {
        return { ok: false, changed: false, value: serialized };
      }
      const nested = config.mcpServers.trim();
      if (nested) {
        try {
          existingServers = JSON.parse(nested);
        } catch (_error) {
          return { ok: false, changed: false, value: serialized };
        }
        if (!Array.isArray(existingServers)) {
          return { ok: false, changed: false, value: serialized };
        }
      }
    }

    const oldManagedIds = previousManagedIds();
    const currentManagedIds = new Set(catalog.services.map(function (service) {
      return service.id;
    }));
    const managedById = new Map();
    const managedByUrl = new Map();
    const preserved = existingServers.filter(function (entry) {
      const isManaged = entry && typeof entry === 'object'
        && ((typeof entry.id === 'string'
             && (oldManagedIds.has(entry.id) || currentManagedIds.has(entry.id)))
            || hasManagedVirtualUrl(entry));
      if (isManaged) {
        if (typeof entry.id === 'string') {
          managedById.set(entry.id, entry);
        }
        if (typeof entry.url === 'string') {
          managedByUrl.set(entry.url, entry);
        }
      }
      return !isManaged;
    });
    const managed = catalog.services.map(function (service) {
      const previous = managedById.get(service.id) || managedByUrl.get(service.virtualUrl);
      const merged = previous && typeof previous === 'object' ? Object.assign({}, previous) : {};
      delete merged.headers;
      merged.id = service.id;
      merged.displayName = service.displayName;
      merged.enabled = service.state === 'running';
      merged.url = service.virtualUrl;
      merged.useProxy = false;
      return merged;
    });
    const mergedServers = preserved.concat(managed);
    const mergedServersJson = JSON.stringify(mergedServers);
    const oldServersJson = Object.prototype.hasOwnProperty.call(config, 'mcpServers')
      ? config.mcpServers
      : null;
    const changed = oldServersJson !== mergedServersJson;
    if (changed) {
      config.mcpServers = mergedServersJson;
    }
    const result = changed ? JSON.stringify(config) : serialized;
    return { ok: true, changed: changed, value: result };
  }

  function rememberManagedIds(catalog) {
    originalSetItem.call(localStorage, MANAGED_IDS_KEY, JSON.stringify(catalog.services.map(function (service) {
      return service.id;
    })));
  }

  function dispatchConfigChanged(oldValue, newValue) {
    try {
      window.dispatchEvent(new StorageEvent('storage', {
        key: CONFIG_KEY,
        oldValue: oldValue,
        newValue: newValue,
        storageArea: localStorage,
        url: window.location.href
      }));
    } catch (_error) {
      window.dispatchEvent(new Event('qcopilots-mcp-catalog-changed'));
    }
  }

  function scheduleFirstVisitReload() {
    if (!firstVisitAtDocumentCreation) {
      return;
    }
    try {
      if (sessionStorage.getItem(FIRST_VISIT_RELOAD_KEY) === '1') {
        return;
      }
      sessionStorage.setItem(FIRST_VISIT_RELOAD_KEY, '1');
    } catch (_error) {
      return;
    }
    window.setTimeout(function () {
      window.location.reload();
    }, 0);
  }

  function mergeStoredConfig(allowFirstVisitReload) {
    if (autoRegistrationDisabled || !catalogActivated || !currentCatalog) {
      return false;
    }
    const existing = originalGetItem.call(localStorage, CONFIG_KEY);
    const creatingConfig = existing === null;
    if (creatingConfig
        && (!currentCatalog.startupComplete || currentCatalog.services.length === 0)) {
      return false;
    }
    const merged = mergeConfigValue(creatingConfig ? '{}' : existing, currentCatalog);
    if (!merged.ok) {
      disableAutoRegistrationForSession();
      return false;
    }
    rememberManagedIds(currentCatalog);
    if (!merged.changed) {
      return false;
    }
    originalSetItem.call(localStorage, CONFIG_KEY, merged.value);
    dispatchConfigChanged(existing, merged.value);
    if (creatingConfig) {
      firstVisitAtDocumentCreation = false;
    }
    if (allowFirstVisitReload && !creatingConfig) {
      scheduleFirstVisitReload();
    }
    return true;
  }

  function scheduleCatalogGenerationReload(generation) {
    if (firstVisitAtDocumentCreation) {
      return;
    }
    try {
      const serializedGeneration = String(generation);
      if (sessionStorage.getItem(CATALOG_RELOAD_GENERATION_KEY) === serializedGeneration) {
        return;
      }
      sessionStorage.setItem(CATALOG_RELOAD_GENERATION_KEY, serializedGeneration);
    } catch (_error) {
      return;
    }
    window.setTimeout(function () {
      window.location.reload();
    }, 0);
  }

  function activateCatalog(dynamicUpdate) {
    if (!currentCatalog
        || (!currentCatalog.startupComplete && currentCatalog.services.length === 0)) {
      return;
    }
    catalogActivated = true;
    const changed = mergeStoredConfig(firstConfigWriteObserved);
    if (changed && dynamicUpdate) {
      scheduleCatalogGenerationReload(currentCatalog.generation);
    }
  }

  function acceptCatalog(serialized, synchronousInitialCatalog) {
    const parsed = parsePublicCatalog(serialized);
    if (!parsed) {
      return;
    }
    if (currentCatalog && parsed.generation <= currentCatalog.generation) {
      return;
    }
    currentCatalog = parsed;
    if (catalogActivated || synchronousInitialCatalog || parsed.startupComplete || catalogWaitExpired) {
      activateCatalog(!synchronousInitialCatalog);
    }
  }

  Storage.prototype.setItem = function (key, value) {
    const normalizedKey = String(key);
    const normalizedValue = String(value);
    if (this !== localStorage || normalizedKey !== CONFIG_KEY) {
      return originalSetItem.call(this, normalizedKey, normalizedValue);
    }

    if (firstVisitAtDocumentCreation && !firstConfigWriteObserved) {
      firstConfigWriteObserved = true;
    }
    if (autoRegistrationDisabled || !catalogActivated || !currentCatalog) {
      return originalSetItem.call(this, normalizedKey, normalizedValue);
    }

    const merged = mergeConfigValue(normalizedValue, currentCatalog);
    if (!merged.ok) {
      disableAutoRegistrationForSession();
      return originalSetItem.call(this, normalizedKey, normalizedValue);
    }
    rememberManagedIds(currentCatalog);
    const result = originalSetItem.call(this, normalizedKey, merged.value);
    if (firstVisitAtDocumentCreation && merged.changed) {
      scheduleFirstVisitReload();
    }
    return result;
  };

  function isVirtualMcpUrl(url) {
    return url.origin === VIRTUAL_ORIGIN
      && url.username === ''
      && url.password === ''
      && url.search === ''
      && url.hash === ''
      && /^\/mcp\/[^/]+$/.test(url.pathname);
  }

  function targetsVirtualMcpHost(url) {
    const hostname = url.hostname.toLowerCase();
    return hostname === VIRTUAL_HOST || hostname === VIRTUAL_HOST + '.';
  }

  function isCurrentRunningVirtualMcpUrl(url) {
    return !!currentCatalog && currentCatalog.services.some(function (service) {
      return service.state === 'running' && service.virtualUrl === url.href;
    });
  }

  function rejectVirtualMcpRequest(message) {
    return Promise.reject(new TypeError(message));
  }

  function requestUrl(input) {
    if (input instanceof Request) {
      return new URL(input.url);
    }
    if (input instanceof URL) {
      return new URL(input.href);
    }
    return new URL(String(input), document.baseURI);
  }

  function nextRequestId() {
    requestCounter += 1;
    if (window.crypto && typeof window.crypto.randomUUID === 'function') {
      return 'qcopilots-' + window.crypto.randomUUID();
    }
    return 'qcopilots-' + Date.now().toString(36) + '-' + requestCounter.toString(36);
  }

  function base64Bytes(value) {
    const decoded = window.atob(value || '');
    const bytes = new Uint8Array(decoded.length);
    for (let index = 0; index < decoded.length; ++index) {
      bytes[index] = decoded.charCodeAt(index);
    }
    return bytes;
  }

  function settlePending(requestId, callback) {
    const pending = pendingRequests.get(requestId);
    if (!pending) {
      return;
    }
    pendingRequests.delete(requestId);
    if (pending.signal && pending.abortHandler) {
      pending.signal.removeEventListener('abort', pending.abortHandler);
    }
    callback(pending);
  }

  function waitForBridge(signal) {
    if (signal.aborted) {
      return Promise.reject(new DOMException('The operation was aborted.', 'AbortError'));
    }

    return new Promise(function (resolve, reject) {
      let settled = false;
      const abortHandler = function () {
        if (settled) {
          return;
        }
        settled = true;
        signal.removeEventListener('abort', abortHandler);
        reject(new DOMException('The operation was aborted.', 'AbortError'));
      };
      const finish = function (callback, value) {
        if (settled) {
          return;
        }
        settled = true;
        signal.removeEventListener('abort', abortHandler);
        callback(value);
      };

      signal.addEventListener('abort', abortHandler, { once: true });
      if (signal.aborted) {
        abortHandler();
        return;
      }
      bridgeReady.then(
        function (bridge) { finish(resolve, bridge); },
        function (error) { finish(reject, error); }
      );
    });
  }

  function requestTimeoutError() {
    return new TypeError('The local MCP request timed out before it could be sent.');
  }

  function remainingRequestTimeout(deadline) {
    return Math.ceil(deadline - Date.now());
  }

  function cancelBodyReader(reader) {
    try {
      Promise.resolve(reader.cancel()).catch(function () {});
    } catch (_error) {
      // Cancellation is best effort after the request has already failed.
    }
  }

  function readBodyChunk(reader, signal, deadline) {
    if (signal.aborted) {
      cancelBodyReader(reader);
      return Promise.reject(new DOMException('The operation was aborted.', 'AbortError'));
    }

    const timeoutMs = remainingRequestTimeout(deadline);
    if (timeoutMs <= 0) {
      cancelBodyReader(reader);
      return Promise.reject(requestTimeoutError());
    }

    return new Promise(function (resolve, reject) {
      let settled = false;
      let timeoutId = null;
      const finish = function (callback, value, cancelReader) {
        if (settled) {
          return;
        }
        settled = true;
        signal.removeEventListener('abort', abortHandler);
        if (timeoutId !== null) {
          window.clearTimeout(timeoutId);
        }
        if (cancelReader) {
          cancelBodyReader(reader);
        }
        callback(value);
      };
      const abortHandler = function () {
        finish(
          reject,
          new DOMException('The operation was aborted.', 'AbortError'),
          true
        );
      };

      signal.addEventListener('abort', abortHandler, { once: true });
      if (signal.aborted) {
        abortHandler();
        return;
      }
      timeoutId = window.setTimeout(function () {
        finish(reject, requestTimeoutError(), true);
      }, timeoutMs);
      reader.read().then(
        function (result) { finish(resolve, result, false); },
        function (error) { finish(reject, error, false); }
      );
    });
  }

  async function readRequestBody(request, deadline) {
    if (!request.body) {
      return '';
    }

    const reader = request.body.getReader();
    const chunks = [];
    let totalBytes = 0;
    try {
      while (true) {
        const result = await readBodyChunk(reader, request.signal, deadline);
        if (result.done) {
          break;
        }
        const chunk = result.value instanceof Uint8Array
          ? result.value
          : new Uint8Array(result.value);
        totalBytes += chunk.byteLength;
        if (totalBytes > MAX_BODY_BYTES) {
          cancelBodyReader(reader);
          throw new TypeError('The local MCP request body exceeds the 1 MiB limit.');
        }
        chunks.push(chunk);
      }
    } finally {
      try {
        reader.releaseLock();
      } catch (_error) {
        // The reader may still be finishing asynchronous cancellation.
      }
    }

    const bodyBytes = new Uint8Array(totalBytes);
    let offset = 0;
    for (const chunk of chunks) {
      bodyBytes.set(chunk, offset);
      offset += chunk.byteLength;
    }
    return new TextDecoder().decode(bodyBytes);
  }

  async function nativeMcpFetch(input, init) {
    const deadline = Date.now() + REQUEST_TIMEOUT_MS;
    const request = new Request(input, init);
    const method = request.method.toUpperCase();
    if (method !== 'GET' && method !== 'POST' && method !== 'DELETE') {
      throw new TypeError('Only GET, POST, and DELETE are allowed for local MCP requests.');
    }
    if (request.signal.aborted) {
      throw new DOMException('The operation was aborted.', 'AbortError');
    }
    const bridge = await waitForBridge(request.signal);

    const forwardedHeaders = {};
    for (const pair of request.headers.entries()) {
      const name = pair[0].toLowerCase();
      if (name === 'authorization') {
        continue;
      }
      if (!ALLOWED_REQUEST_HEADERS.has(name)) {
        throw new TypeError('Unsupported local MCP request header: ' + pair[0]);
      }
      forwardedHeaders[name] = pair[1];
    }

    let body = '';
    if (method === 'POST') {
      body = await readRequestBody(request, deadline);
      if (request.signal.aborted) {
        throw new DOMException('The operation was aborted.', 'AbortError');
      }
    }

    const timeoutMs = remainingRequestTimeout(deadline);
    if (timeoutMs < 100) {
      throw requestTimeoutError();
    }

    const requestId = nextRequestId();
    return new Promise(function (resolve, reject) {
      const abortHandler = function () {
        bridge.cancel(requestId);
        settlePending(requestId, function (pending) {
          pending.reject(new DOMException('The operation was aborted.', 'AbortError'));
        });
      };
      pendingRequests.set(requestId, {
        resolve: resolve,
        reject: reject,
        signal: request.signal,
        abortHandler: abortHandler
      });
      request.signal.addEventListener('abort', abortHandler, { once: true });
      if (request.signal.aborted) {
        abortHandler();
        return;
      }
      bridge.request(requestId, method, request.url, forwardedHeaders, body, timeoutMs);
    });
  }

  window.fetch = function (input, init) {
    let url;
    try {
      url = requestUrl(input);
    } catch (error) {
      return Promise.reject(error);
    }
    if (!targetsVirtualMcpHost(url)) {
      return originalFetch(input, init);
    }
    if (!isVirtualMcpUrl(url)) {
      return rejectVirtualMcpRequest('The local MCP virtual URL format is invalid.');
    }
    if (!isCurrentRunningVirtualMcpUrl(url)) {
      return rejectVirtualMcpRequest('The local MCP virtual URL is not present in the current running catalog.');
    }
    return nativeMcpFetch(input, init);
  };

  function installBridge(bridge) {
    if (!bridge || nativeBridge) {
      return;
    }
    nativeBridge = bridge;
    bridge.response.connect(function (requestId, status, statusText, headers, bodyBase64) {
      settlePending(requestId, function (pending) {
        if (pending.signal && pending.signal.aborted) {
          pending.reject(new DOMException('The operation was aborted.', 'AbortError'));
          return;
        }
        try {
          const noBodyStatus = status === 204 || status === 205 || status === 304;
          pending.resolve(new Response(noBodyStatus ? null : base64Bytes(bodyBase64), {
            status: status,
            statusText: statusText,
            headers: new Headers(headers || {})
          }));
        } catch (error) {
          pending.reject(error);
        }
      });
    });
    bridge.requestFailed.connect(function (requestId, code, message) {
      settlePending(requestId, function (pending) {
        if (code === 'canceled') {
          pending.reject(new DOMException(message, 'AbortError'));
        } else {
          pending.reject(new TypeError(message));
        }
      });
    });
    bridge.catalogChanged.connect(function (serialized) {
      acceptCatalog(serialized, false);
    });
    bridge.catalog(function (serialized) {
      acceptCatalog(serialized, false);
    });
    resolveBridge(bridge);
  }

  window.setTimeout(function () {
    catalogWaitExpired = true;
    if (currentCatalog) {
      activateCatalog(true);
    }
  }, CATALOG_WAIT_MS);

  if (typeof window.__qcopilotsInitialMcpCatalogV1 === 'string') {
    acceptCatalog(window.__qcopilotsInitialMcpCatalogV1, true);
  }

  if (window.qt && window.qt.webChannelTransport && typeof window.QWebChannel === 'function') {
    try {
      new window.QWebChannel(window.qt.webChannelTransport, function (channel) {
        installBridge(channel.objects.qcopilotsMcpBridge);
      });
    } catch (error) {
      rejectBridge(error);
    }
  } else {
    rejectBridge(new Error('Qt WebChannel is unavailable in the QCopilots page.'));
  }
})();
