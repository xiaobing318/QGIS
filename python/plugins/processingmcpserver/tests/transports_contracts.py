from __future__ import annotations

from processingmcpserver.transports import StreamableHttpTransport, create_transport

from ._shared_case_base import ProcessingMCPTestBase


class TransportsContractsTest(ProcessingMCPTestBase):
    def test_transport_fallback_to_streamable_http(self):
        transport = create_transport(object(), self._build_config("invalid-transport"))
        self.assertIsInstance(transport, StreamableHttpTransport)
