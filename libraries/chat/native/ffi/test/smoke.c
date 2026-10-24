/* Links the static library the way the viewer will and drives it through the C interface: a fresh folder starts
 * signed out, a wrong command comes back as an error event, and free returns. Built as C and as C++ (the viewer). */
#include <stdio.h>
#include <string.h>
#include "darkride_chat.h"
#ifdef _WIN32
#include <windows.h>
static void pause_ms(int ms) { Sleep(ms); }
#else
#include <unistd.h>
static void pause_ms(int ms) { usleep(ms * 1000); }
#endif

static volatile int signed_out = 0, errored = 0;

static void on_event(const char* event, void* user_data) {
	(void)user_data;
	printf("event: %s\n", event);
	if (strstr(event, "\"signed_out\"")) signed_out = 1;
	if (strstr(event, "\"event\":\"error\"") && strstr(event, "no such command")) errored = 1;
}

int main(int argc, char** argv) {
	char config[1024];
	snprintf(config, sizeof config, "{\"homeserver\": \"http://127.0.0.1:8008\", \"data_dir\": \"%s\"}", argc > 1 ? argv[1] : "smoke-data");
	printf("version %s\n", darkride_chat_version());
	DarkrideChat* chat = darkride_chat_new(config, on_event, NULL);
	if (!chat) { printf("FAIL  darkride_chat_new\n"); return 1; }
	for (int i = 0; i < 100 && !signed_out; i++) pause_ms(50);
	darkride_chat_command(chat, "{\"op\": \"dance\"}");
	for (int i = 0; i < 100 && !errored; i++) pause_ms(50);
	darkride_chat_free(chat);
	printf("%s  starts signed out\n%s  a wrong command is an error event\n", signed_out ? "PASS" : "FAIL", errored ? "PASS" : "FAIL");
	printf("%s\n", darkride_chat_new("not json", on_event, NULL) == NULL ? "PASS  a wrong config gives NULL" : "FAIL  a wrong config");
	return signed_out && errored ? 0 : 1;
}
