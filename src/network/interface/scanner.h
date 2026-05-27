#pragma once

#include "network/types.h"
#include <queue>

class INetworkScanner {
public:
    virtual ~INetworkScanner() = default;
    virtual bool is_initialized_successfully() const = 0;
    virtual void search_lobbies(const char* query) = 0;
    virtual void service() = 0;

    bool poll_lobbies(NetworkLobby* lobby);
protected:
    char scanner_lobby_name_query[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    std::queue<NetworkLobby> scanner_lobbies;
};
