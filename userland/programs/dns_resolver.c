#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *resolve_name(const char *host)
{
    if (!host) return "unknown";
    if (strcmp(host, "example.com") == 0) return "93.184.216.34";
    if (strcmp(host, "localhost") == 0) return "127.0.0.1";
    return "0.0.0.0";
}

int dns_resolver_main(const char *host)
{
    const char *ip = resolve_name(host);
    char buf[64];
    snprintf(buf, sizeof(buf), "DNS %s -> %s", host ? host : "", ip);
    (void)buf;
    return 0;
}
