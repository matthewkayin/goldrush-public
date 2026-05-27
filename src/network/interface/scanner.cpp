#include "scanner.h"

bool INetworkScanner::poll_lobbies(NetworkLobby* lobby) {
    if (scanner_lobbies.empty()) {
        return false;
    }

    *lobby = scanner_lobbies.front();
    scanner_lobbies.pop();

    return true;
}
