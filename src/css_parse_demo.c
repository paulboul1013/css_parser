#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.css>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror(argv[1]);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fprintf(stderr, "Out of memory\n");
        fclose(fp);
        return 1;
    }

    size_t nread = fread(buf, 1, (size_t)size, fp);
    buf[nread] = '\0';
    fclose(fp);

    printf("Read %zu bytes from %s\n", nread, argv[1]);
    printf("TODO: tokenize and parse\n");

    free(buf);
    return 0;
}
