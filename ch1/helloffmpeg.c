#include <stdio.h>
#include <libavutil/avutil.h>

int main(int argc, char *argv[])
{
	int i = 10;
	printf("%d\n", i);
	av_log(NULL, AV_LOG_INFO, "Hello World by vscode\n");
	return 0;
}
