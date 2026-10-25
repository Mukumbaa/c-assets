#define LIBINI_IMPLEMENTATION
#include "../standalone_lib/libini.h"
#include <stdio.h>

typedef struct {
    char env[32];
    bool maintenance_mode;

    // [server]
    char server_ip[64];
    int server_port;
    int max_conns;
    double timeout;
    char tls_mode;

    // [database]
    char db_driver[16];
    char db_host[64];
    int db_port;
    char db_user[64];
    char db_pass[64];
    int db_pool_size;

    // [security]
    bool enable_cors;
    int max_login_tries;
    double jwt_expiry_min;

    // [logging]
    char log_level[16];
    bool log_to_file;
    char log_path[128];
} AppConfig;

int main(void) {
    AppConfig cfg = {
        .env              = "development",
        .maintenance_mode = false,

        .server_ip        = "127.0.0.1",
        .server_port      = 8080,
        .max_conns        = 500,
        .timeout          = 30.0,
        .tls_mode         = 'd',

        .db_driver        = "sqlite",
        .db_host          = "localhost",
        .db_port          = 5432,
        .db_user          = "root",
        .db_pass          = "",
        .db_pool_size     = 10,

        .enable_cors      = false,
        .max_login_tries  = 3,
        .jwt_expiry_min   = 15.0,

        .log_level        = "warn",
        .log_to_file      = false,
        .log_path         = "app.log"
    };

    IniParser p;
    ini_init(&p);

    ini_bind_str(&p, NULL, "environment", cfg.env, sizeof(cfg.env), false, "development,staging,production");
    ini_bind_bool(&p, NULL, "maintenance_mode", &cfg.maintenance_mode, false);

    ini_bind_str(&p, "server", "bind_ip", cfg.server_ip, sizeof(cfg.server_ip), false, NULL);
    ini_bind_int(&p, "server", "port", &cfg.server_port, true, NULL); // OBBLIGATORIA
    ini_bind_int(&p, "server", "max_conns", &cfg.max_conns, false, NULL);
    ini_bind_float(&p, "server", "timeout", &cfg.timeout, false, NULL);
    ini_bind_char(&p, "server", "tls_mode", &cfg.tls_mode, false, "s,m,d");

    ini_bind_str(&p, "database", "driver", cfg.db_driver, sizeof(cfg.db_driver), true, "postgres,mysql,sqlite");
    ini_bind_str(&p, "database", "host", cfg.db_host, sizeof(cfg.db_host), true, NULL); // OBBLIGATORIA
    ini_bind_int(&p, "database", "port", &cfg.db_port, false, NULL);
    ini_bind_str(&p, "database", "username", cfg.db_user, sizeof(cfg.db_user), true, NULL); // OBBLIGATORIA
    ini_bind_str(&p, "database", "password", cfg.db_pass, sizeof(cfg.db_pass), true, NULL); // OBBLIGATORIA
    ini_bind_int(&p, "database", "pool_size", &cfg.db_pool_size, false, NULL);

    ini_bind_bool(&p, "security", "enable_cors", &cfg.enable_cors, false);
    ini_bind_int(&p, "security", "max_login_tries", &cfg.max_login_tries, false, NULL);
    ini_bind_float(&p, "security", "jwt_expiry_min", &cfg.jwt_expiry_min, false, NULL);

    ini_bind_str(&p, "logging", "level", cfg.log_level, sizeof(cfg.log_level), false, "debug,info,warn,error");
    ini_bind_bool(&p, "logging", "file_output", &cfg.log_to_file, false);
    ini_bind_str(&p, "logging", "log_path", cfg.log_path, sizeof(cfg.log_path), false, NULL);

    const char *config_file = "test.ini";
    IniStatus status = ini_parse_file(&p, config_file);

    if (status != INI_OK) {
        fprintf(stderr, "Config error in '%s':\n  --> %s\n", config_file, ini_get_error(&p));
        ini_free(&p);
        return 1;
    }

    ini_free(&p);

    printf("=========================================\n");
    printf("     APPLICATION CONFIGURATION LOADED    \n");
    printf("=========================================\n");
    printf("[Global]\n");
    printf("  Environment:     %s\n", cfg.env);
    printf("  Maintenance:     %s\n\n", cfg.maintenance_mode ? "YES" : "NO");

    printf("[server]\n");
    printf("  Listening on:    %s:%d\n", cfg.server_ip, cfg.server_port);
    printf("  Max connections: %d\n", cfg.max_conns);
    printf("  Timeout:         %.1fs\n", cfg.timeout);
    printf("  TLS mode:        %c\n\n", cfg.tls_mode);

    printf("[database]\n");
    printf("  Driver:          %s\n", cfg.db_driver);
    printf("  Connecting to:   %s@%s:%d\n", cfg.db_user, cfg.db_host, cfg.db_port);
    printf("  Password:        %s\n", cfg.db_pass);
    printf("  Pool size:       %d\n\n", cfg.db_pool_size);

    printf("[security]\n");
    printf("  CORS enabled:    %s\n", cfg.enable_cors ? "SI" : "NO");
    printf("  Max attempts:    %d\n", cfg.max_login_tries);
    printf("  JWT expiration:  %.1f min\n\n", cfg.jwt_expiry_min);

    printf("[logging]\n");
    printf("  Log level:       %s\n", cfg.log_level);
    printf("  File output:     %s (%s)\n", cfg.log_to_file ? "YES" : "NO", cfg.log_path);
    printf("=========================================\n");

    return 0;
}
