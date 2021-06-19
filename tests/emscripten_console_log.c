#include <assert.h>
#include <emscripten.h>
#include <emscripten/html5.h>

int main()
{
	emscripten_console_log("Hello!");
	emscripten_console_warn("Hello!");
	emscripten_console_error("Hello!");
	assert(EM_ASM_INT(return Module['testPassed']));
	return 0;
}
