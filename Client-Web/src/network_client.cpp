#include "network_client.h"

#include <cstdlib>

#include <emscripten.h>

EM_ASYNC_JS(int, WebSocketConnectAsync, (const char* uriPtr), {
    const uri = UTF8ToString(uriPtr);

    if (Module.__yc_ws) {
        try { Module.__yc_ws.close(); } catch (e) {}
        Module.__yc_ws = null;
    }

    return await new Promise((resolve) => {
        try {
            const ws = new WebSocket(uri);
            ws.binaryType = 'arraybuffer';

            ws.onopen = () => {
                Module.__yc_ws = ws;
                resolve(1);
            };

            ws.onerror = () => {
                resolve(0);
            };

            ws.onclose = () => {
                if (Module.__yc_ws === ws) {
                    Module.__yc_ws = null;
                }
            };
        } catch (e) {
            resolve(0);
        }
    });
});

EM_ASYNC_JS(char*, WebSocketRequestAsync, (const char* payloadPtr, int timeoutMs), {
    const ws = Module.__yc_ws;
    if (!ws || ws.readyState !== 1) {
        return 0;
    }

    const payload = UTF8ToString(payloadPtr);

    const response = await new Promise((resolve) => {
        let done = false;

        const cleanup = () => {
            ws.removeEventListener('message', onMessage);
            ws.removeEventListener('error', onError);
        };

        const finish = (value) => {
            if (done) return;
            done = true;
            cleanup();
            resolve(value);
        };

        const timer = setTimeout(() => finish(null), timeoutMs);

        const onMessage = (event) => {
            clearTimeout(timer);
            if (typeof event.data === 'string') {
                finish(event.data);
            } else {
                finish(null);
            }
        };

        const onError = () => {
            clearTimeout(timer);
            finish(null);
        };

        ws.addEventListener('message', onMessage);
        ws.addEventListener('error', onError);

        try {
            ws.send(payload);
        } catch (e) {
            clearTimeout(timer);
            finish(null);
        }
    });

    if (response === null) {
        return 0;
    }

    const len = lengthBytesUTF8(response) + 1;
    const ptr = _malloc(len);
    stringToUTF8(response, ptr, len);
    return ptr;
});

EM_JS(void, WebSocketDisconnectJs, (), {
    if (Module.__yc_ws) {
        try { Module.__yc_ws.close(); } catch (e) {}
        Module.__yc_ws = null;
    }
});

namespace yc {

NetworkClient& NetworkClient::instance() {
    static NetworkClient instance;
    return instance;
}

NetworkClient::NetworkClient() = default;

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect(const std::string& uri, std::string* error) {
    disconnect();

    const int ok = WebSocketConnectAsync(uri.c_str());
    connected.store(ok != 0);

    if (!connected.load() && error) {
        *error = "WebSocket connection failed";
    }

    return connected.load();
}

void NetworkClient::disconnect() {
    WebSocketDisconnectJs();
    connected.store(false);
}

bool NetworkClient::request(const std::string& payload, std::string& response, int timeoutMs) {
    if (!connected.load()) {
        return false;
    }

    char* responsePtr = WebSocketRequestAsync(payload.c_str(), timeoutMs);
    if (!responsePtr) {
        return false;
    }

    response = responsePtr;
    std::free(responsePtr);
    return true;
}

}
