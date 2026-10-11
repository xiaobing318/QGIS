"""General Processing remains usable when transfer state cannot be opened."""

import sys
import threading
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "python" / "plugins"))

from qcopilots_common import processing_tools
from qcopilots_mcp_server_processing_general import server
from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError
from qcopilots_mcp_server_processing_general.file_transfer_tools import FileTransferManagerProvider


class TestFileTransferServiceLifecycle(unittest.TestCase):
    def test_processing_survives_unavailable_transfer_storage(self):
        factory = mock.Mock(side_effect=PermissionError("transfer storage unavailable"))
        provider = FileTransferManagerProvider(factory=factory)
        self.addCleanup(provider.shutdown)
        bridge = mock.Mock()
        bridge.call.return_value = {"algorithms": []}
        with mock.patch.object(processing_tools, "_bridge_client", return_value=bridge):
            tools = {tool.name: tool for tool in server.build_general_tools(manager_provider=provider)}
        self.assertEqual(len(tools), 17)
        factory.assert_not_called()
        self.assertEqual(tools["list_general_processing_algorithms"].handler({}), {"algorithms": []})
        result = tools["file_transfer_get_config"].handler({})
        self.assertTrue(result["isError"])
        self.assertNotIn("transfer storage unavailable", str(result))
        self.assertEqual(tools["list_general_processing_algorithms"].handler({}), {"algorithms": []})
        self.assertEqual(bridge.call.call_count, 2)

    def test_concurrent_first_calls_share_one_manager(self):
        manager = mock.Mock()
        factory = mock.Mock(return_value=manager)
        provider = FileTransferManagerProvider(factory=factory)
        self.addCleanup(provider.shutdown)
        barrier = threading.Barrier(8)
        results = []

        def request():
            barrier.wait(timeout=5)
            results.append(provider.get())

        workers = [threading.Thread(target=request) for _ in range(8)]
        for worker in workers:
            worker.start()
        for worker in workers:
            worker.join(timeout=5)
            self.assertFalse(worker.is_alive())
        self.assertEqual(results, [manager] * 8)
        factory.assert_called_once_with()
        provider.shutdown()
        provider.shutdown()
        manager.shutdown.assert_called_once_with()
        with self.assertRaises(FileTransferError) as error:
            provider.get()
        self.assertEqual(error.exception.code, "service_stopping")

    def test_shutdown_without_file_transfer_call_does_not_create_storage(self):
        factory = mock.Mock()
        provider = FileTransferManagerProvider(factory=factory)
        provider.shutdown()
        factory.assert_not_called()
        with self.assertRaises(FileTransferError):
            provider.get()
        factory.assert_not_called()

    def test_failed_initialization_can_be_retried(self):
        manager = mock.Mock()
        provider = FileTransferManagerProvider(factory=mock.Mock(side_effect=[PermissionError(), manager]))
        self.addCleanup(provider.shutdown)
        with self.assertRaises(PermissionError):
            provider.get()
        self.assertIs(provider.get(), manager)

    def test_server_shutdown_runs_when_http_server_fails(self):
        provider = FileTransferManagerProvider(factory=mock.Mock())
        with mock.patch.object(server, "FileTransferManagerProvider", return_value=provider), \
                mock.patch.object(server, "run_mcp_server", side_effect=RuntimeError("server stopped")), \
                mock.patch.object(server, "configure_logger"), \
                mock.patch.object(processing_tools, "_bridge_client"):
            with self.assertRaisesRegex(RuntimeError, "server stopped"):
                server.main()
        provider._factory.assert_not_called()
        with self.assertRaises(FileTransferError):
            provider.get()


if __name__ == "__main__":
    unittest.main()
