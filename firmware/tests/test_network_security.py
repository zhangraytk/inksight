"""Run the production token transport code with host-side network doubles.

No device, Python packages or live server are required. The doubles simulate a
rejected certificate; they do not validate the ESP32 TLS implementation itself.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
NETWORK = ROOT / "src" / "inksight_net.cpp"


def cpp_function(source, signature):
    start = source.index(signature + " {")
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


STUBS = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct String {
    std::string value;
    String(const char *s = "") : value(s) {}
    String(std::string s) : value(s) {}
    size_t length() const { return value.size(); }
    const char *c_str() const { return value.c_str(); }
    char operator[](size_t i) const { return value[i]; }
    bool startsWith(const char *s) const { return value.rfind(s, 0) == 0; }
    int indexOf(const String &s) const {
        auto i = value.find(s.value);
        return i == std::string::npos ? -1 : static_cast<int>(i);
    }
    int indexOf(char c, size_t start) const {
        auto i = value.find(c, start);
        return i == std::string::npos ? -1 : static_cast<int>(i);
    }
    String substring(size_t start, size_t end) const {
        return value.substr(start, end - start);
    }
    String substring(size_t start) const { return value.substr(start); }
    void replace(const char *from, const char *to) {
        auto i = value.find(from);
        if (i != std::string::npos) value.replace(i, strlen(from), to);
    }
};
String operator+(const String &a, const String &b) { return a.value + b.value; }

struct SerialDouble {
    template <typename... Args> void printf(const char *, Args...) {}
    void println(const char *) {}
} Serial;
constexpr int WL_CONNECTED = 3;
struct WiFiDouble {
    int status() const { return WL_CONNECTED; }
    String macAddress() const { return "AA:BB:CC:DD:EE:FF"; }
} WiFi;
void delay(int) {}
bool checkAbort() { return false; }
const char *ROOT_CA = "test-root-ca";
const char *DEFAULT_USER_AGENT = "test-agent";
constexpr int HTTP_TIMEOUT = 30000;
String cfgServer, cfgDeviceToken;
void saveDeviceToken(const String &token) { cfgDeviceToken = token; }

struct WiFiClient {
    virtual ~WiFiClient() = default;
    virtual bool secure() const { return false; }
    bool insecure = false;
    bool hasCA = false;
    void stop() {}
};
struct WiFiClientSecure : WiFiClient {
    bool secure() const override { return true; }
    void setCACert(const char *ca) { hasCA = ca == ROOT_CA; }
    void setInsecure() { insecure = true; }
    void lastError(char *buf, size_t size) { snprintf(buf, size, "certificate rejected"); }
};
struct Request { bool secure, insecure, hasCA; std::string url; };
std::vector<Request> requests;
bool certificateAccepted = false;
struct HTTPClient {
    WiFiClient *client = nullptr;
    String url;
    bool begin(WiFiClient &c, const String &u) { client = &c; url = u; return true; }
    void setUserAgent(const char *) {}
    void addHeader(const char *, const char *) {}
    void setTimeout(int) {}
    int POST(const char *) {
        requests.push_back({client->secure(), client->insecure, client->hasCA, url.value});
        if (!client->secure() || client->insecure) return 200;
        return certificateAccepted && client->hasCA ? 200 : -1;
    }
    String getString() { return "{\"token\":\"test-device-token\"}"; }
    String errorToString(int) { return "connection failed"; }
    void end() {}
};
struct WebSocketDouble {
    bool secure = false, insecure = false, hasCA = false;
    void beginSSL(const char *, uint16_t, const char *, const char *, const char *) {
        secure = true; insecure = true; hasCA = false;
    }
    void beginSslWithCA(const char *, uint16_t, const char *, const char *ca, const char *) {
        secure = true; insecure = false; hasCA = ca == ROOT_CA;
    }
    void begin(const char *, uint16_t, const char *, const char *) {
        secure = false; insecure = false; hasCA = false;
    }
} gVoiceWsClient;
'''

CASES = r'''
void reset(const char *server, bool insecure, bool validCertificate) {
    cfgServer = server;
    cfgDeviceToken = "";
    cfgInsecureTls = insecure;
    certificateAccepted = validCertificate;
    requests.clear();
}
int main() {
    assert(!cfgInsecureTls); // Compile the actual production initializer.
    reset("https://test.invalid", false, false);
    assert(!ensureDeviceToken());
    assert(requests.size() == 3);
    for (const auto &r : requests) {
        assert(r.secure && !r.insecure && r.hasCA);
        assert(r.url.rfind("https://", 0) == 0);
    }
    assert(cfgDeviceToken.length() == 0);

    reset("https://test.invalid:8443/base", false, false);
    assert(!ensureDeviceToken());
    assert(requests.size() == 3);
    for (const auto &r : requests) {
        assert(r.secure && !r.insecure && r.hasCA);
        assert(r.url.rfind("https://test.invalid:8443/base/", 0) == 0);
    }

    reset("https://test.invalid", false, true);
    assert(ensureDeviceToken());
    assert(requests.size() == 1 && requests[0].hasCA && !requests[0].insecure);
    assert(cfgDeviceToken.value == "test-device-token");

    reset("https://test.invalid", true, false);
    assert(ensureDeviceToken()); // Explicit debug opt-in still works.
    assert(requests.size() == 1 && requests[0].secure && requests[0].insecure);

    reset("http://test.internal", false, false);
    assert(ensureDeviceToken()); // Explicit local HTTP configuration still works.
    assert(requests.size() == 1 && !requests[0].secure);

    cfgInsecureTls = false;
    beginVoiceForTest(true);
    assert(gVoiceWsClient.secure && gVoiceWsClient.hasCA && !gVoiceWsClient.insecure);
    cfgInsecureTls = true;
    beginVoiceForTest(true);
    assert(gVoiceWsClient.secure && !gVoiceWsClient.hasCA && gVoiceWsClient.insecure);
    cfgInsecureTls = false;
    beginVoiceForTest(false);
    assert(!gVoiceWsClient.secure);
}
'''


class NetworkSecurityTests(unittest.TestCase):
    def test_token_transport_policy(self):
        compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
        self.assertIsNotNone(compiler, "A host C++ compiler is required")
        source = NETWORK.read_text()
        storage = (ROOT / "src" / "storage.cpp").read_text()
        initializer = next(line for line in storage.splitlines()
                           if line.startswith("bool   cfgInsecureTls ="))
        functions = [
            cpp_function(source, "static bool beginHttpForUrl(HTTPClient &http, WiFiClient &plainClient, WiFiClientSecure &secClient, const String &url)"),
            cpp_function(source, "static String extractJsonStringField(const String &body, const char *key)"),
            cpp_function(source, "bool ensureDeviceToken()"),
        ]
        voice_start = source.index("        if (useSSL) {", source.index("bool voiceWsOpen("))
        voice_end = source.index("        gVoiceWsConfigured = true;", voice_start)
        functions.append(
            'void beginVoiceForTest(bool useSSL) { String host="test.invalid", path="/ws"; uint16_t port=443;\n'
            + source[voice_start:voice_end] + "\n}"
        )
        with tempfile.TemporaryDirectory(prefix="inksight-network-", dir=os.getenv("TMPDIR")) as temp:
            cpp = Path(temp) / "test.cpp"
            binary = Path(temp) / "test"
            cpp.write_text(STUBS + initializer + "\n" + "\n".join(functions) + CASES)
            subprocess.run([compiler, "-std=c++17", str(cpp), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    def test_other_paths_cannot_silently_downgrade(self):
        source = NETWORK.read_text()
        self.assertFalse('replace("https://", "http://")' in source, "HTTPS must not downgrade to HTTP")
        self.assertFalse("WiFi.setDNS(" in source, "DHCP DNS must be preserved")
        self.assertIn('gVoiceWsClient.beginSslWithCA(host.c_str(), port, path.c_str(), ROOT_CA, "")', source)
        for function in ["bool connectWiFi()", "bool fetchBMP(int modeStep, bool *isFallback, String *renderedModeIdOut)"]:
            body = cpp_function(source, function)
            if function.startswith("bool connectWiFi"):
                self.assertNotIn("setInsecure", body)
            else:
                # The only insecure render setup must remain an explicit opt-in.
                self.assertEqual(body.count("setInsecure()"), 1)
                self.assertIn("if (cfgInsecureTls) {\n                secClient.setInsecure();", body)

    def test_persisted_and_portal_defaults(self):
        storage = (ROOT / "src" / "storage.cpp").read_text()
        self.assertIn('prefs.getBool("insec_tls", false)', storage)
        portal = (ROOT / "data" / "portal_html.h").read_text()
        checkbox = next(line for line in portal.splitlines() if '<input type="checkbox" id="insecTlsIn"' in line)
        self.assertNotIn("checked", checkbox)


if __name__ == "__main__":
    unittest.main()
