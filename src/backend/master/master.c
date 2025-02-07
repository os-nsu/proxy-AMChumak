#include "../../include/master.h"
#include "../../include/config.h"
#include "../../include/logger.h"
#include <dlfcn.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TYPE DECLARATIONS

/*!
    \struct Plugin
    \brief Pair of dlopen handle and name
*/
struct Plugin {
    void *handle;
    char *name;
};

/*!
    \struct PluginsStack
    \brief Stack for plugin pointers
*/
struct PluginsStack {
    struct Plugin *plugins;
    int size;
    int max_idx;
};

// GLOBALS

Hook executor_start_hook = NULL;
Hook executor_end_hook = NULL;

// FUNCTION DECLARATIONS

struct PluginsStack *init_plugins_stack(int boot_size);
void close_plugins(struct PluginsStack *stack);
int push_plugin(struct PluginsStack *stack, void *plugin, char *name);
struct Plugin pop_plugin(struct PluginsStack *stack);
struct Plugin get_plugin(struct PluginsStack *stack, char *name);
char *get_path_from_arg0(const char *arg0);
char *create_path_from_call_dir(const char *arg0, const char *path);
char *mk_plugin_path(const char *file_name, const char *plugins_dir,
                     const char *arg0);
int load_plugins(char **plugins_list, int plugins_count, char *plugins_dir,
                 struct PluginsStack *stack, const char *arg0);
int parse_args(int argc, char *argv[]);
int parse_envs(void);
int init_config_values(char *exec_path);
int set_program_mode(char *hardlink);
int main(int argc, char **argv);

// FUNCTION DEFINITIONS

/*!
    \brief Initializes stack for plugins
    \param[in] bootSize Start size of stack
    \return Pointer to new stack
*/
struct PluginsStack *init_plugins_stack(int boot_size) {
    struct PluginsStack *stack =
        (struct PluginsStack *)malloc(sizeof(struct PluginsStack));
    stack->max_idx = -1;
    stack->size = 0;
    stack->plugins = (struct Plugin *)calloc(boot_size, sizeof(struct Plugin));
    return stack;
}

/*!
    \brief Finishes all plugins and closes handles
    \param[in] stack Pointer to stack
    \return 0 if success, -1 and sets -1 else;
*/
void close_plugins(struct PluginsStack *stack) {
    void (*fini_plugin)(void) = NULL;
    for (int i = stack->max_idx; i >= 0; i--) {
        fini_plugin = dlsym(stack->plugins[i].handle, "fini");
        fini_plugin();
        dlclose(stack->plugins[i].handle);
    }
}

/*!
    \brief Pushes plugin to stack
    \param[in] stack Pointer to stack
    \param[in] plugin Plugin's pointer
    \param[in] name Plugin's name
    \return 0 if success, -1 else
*/
int push_plugin(struct PluginsStack *stack, void *plugin, char *name) {
    if (stack->max_idx == stack->size - 1) {
        stack->size = (int)(stack->size * 1.1 + 1);
        stack->plugins = (struct Plugin *)realloc(
            stack->plugins, stack->size * sizeof(struct Plugin));
    }
    stack->plugins[++stack->max_idx].handle = plugin;
    stack->plugins[stack->max_idx].name = name;

    return 0;
}

/*!
    \brief pop plugin from stack
    Pops plugin from stack
    \param[in] stack Pointer to stack
    \return plugin if success, NULL else
*/
struct Plugin pop_plugin(struct PluginsStack *stack) {
    struct Plugin result;
    result.handle = NULL;
    result.name = NULL;
    if (stack->size == 0) {
        return result;
    }
    if (!stack->plugins[stack->max_idx].handle) {
        return result;
    }
    return stack->plugins[stack->max_idx--];
}

/*!
    \brief gets plugin by it's name
    \param[in] stack Pointer to plugin stack
    \param[in] name Name of plugin
    \return plugin if success, empty plugin structure else
*/
struct Plugin get_plugin(struct PluginsStack *stack, char *name) {
    struct Plugin result;
    result.handle = NULL;
    result.name = NULL;
    for (int i = 0; i < stack->size; i++) {
        if (strcmp(name, stack->plugins[i].name) == 0) {
            return stack->plugins[i];
        }
    }
    return result;
}

/*!
    \brief creates path from directory, where executable was executed to
    directory with executable
    \param [in] arg0 first in arguments list for main
    \return allocated path to directory with executable from called dir
*/
char *get_path_from_arg0(const char *arg0) {
    if (!arg0)
        return NULL;

    int pos = strlen(arg0) - 1;
    while (pos >= 0) {
        if (arg0[pos] == '/') {
            pos++;
            break;
        }
        pos--;
    }
    if (pos < 0) {
        char *path = (char *)calloc(3, sizeof(char));
        strcpy(path, "./");
        return path;
    }
    char *path = (char *)calloc(pos + 1, sizeof(char));
    strncpy(path, arg0, pos);
    return path;
}

/*!
    \brief creates path from directory, where executable was executed
    \param [in] arg0 first in arguments list for main
    \param [in] path relative path from executable file
    \return allocated path to file from called dir
*/
char *create_path_from_call_dir(const char *arg0, const char *path) {
    if (!arg0 || !path)
        return NULL;

    // prepare first part
    char *exec_dir = get_path_from_arg0(arg0);

    // glue
    int len = strlen(exec_dir) + strlen(path) + 2;
    char *result_path = calloc(len, sizeof(char));
    strcat(strcat(result_path, exec_dir), path);
    free(exec_dir);
    return result_path;
}

/*!
    \brief creates path to plugin
    \param[in] fileName Name of dynamic library
    \param[in] pluginsDir Path to plugins directory (could be NULL) (absolute or
   relative from current work directory) \param[in] arg0 first argument for main
    \return 0 if success, -1 else
*/
char *mk_plugin_path(const char *file_name, const char *plugins_dir,
                     const char *arg0) {
    if (!file_name) {
        return NULL;
    }
    int needed_free = 0;
    if (!plugins_dir) {
        needed_free = 1;
        plugins_dir = create_path_from_call_dir(arg0, "./plugins/");
    }

    char *slash = "";
    if (plugins_dir[strlen(plugins_dir) - 1] != '/') {
        slash = "/";
    }

    char *plugin_path =
        (char *)calloc(strlen(plugins_dir) + strlen(slash) + strlen(file_name) +
                           strlen(".so") + 1,
                       sizeof(char));
    strcat(strcat(strcat(strcat(plugin_path, plugins_dir), slash), file_name),
           ".so");
    if (needed_free)
        free((void *)plugins_dir);
    return plugin_path;
}

/*!
    \brief Loads plugins
    \param[in] plugins_list array of plugin names
    \param[in] plugins_count array size
    \param[in] pluginsDir plugins directory (could be NULL)
    \param[in] stack Stack for plugins
    \param[in] arg0 first argument of main
    \return 0 if success, -1 else
*/
int load_plugins(char **plugins_list, int plugins_count, char *plugins_dir,
                 struct PluginsStack *stack, const char *arg0) {
    if (!plugins_list) {
        return -1;
    }
    void (*init_plugin)(
        void); ///< this function will be executed for each contrib library
    void *handle;
    char *error;

    for (int i = 0; (i < plugins_count); ++i) {
        /*TRY TO LOAD .SO FILE*/

        char *plugin_path = mk_plugin_path(plugins_list[i], plugins_dir, arg0);
        handle = dlopen(plugin_path, RTLD_NOW | RTLD_GLOBAL);

        if (!handle) {
            error = dlerror();
            fprintf(stderr, "Library couldn't be opened.\n \
                \tLibrary's path is %s\n \
                \tdlopen: %s\n \
                \tcheck plugins folder or rename library\n",
                    plugin_path, error);
            free(stack);
            return -1;
        }
        error = dlerror();

        /*PUSH HANDLE TO PLUGIN STACK*/
        push_plugin(stack, handle, plugins_list[i]);

        const char *func_name = "init";
        /*EXECUTE PLUGIN*/
        init_plugin = (void (*)(void))dlsym(handle, func_name);

        error = dlerror();
        if (error != NULL) {
            fprintf(
                stderr,
                "Library couldn't execute %s.\n\tLibrary's name is %s. Dlsym "
                "message: %s\n\tcheck plugins folder or rename library\n",
                func_name, plugins_list[i], error);
            free(stack);
            return -1;
        }

        init_plugin();
    }
    return 0;
}

/*!
    \brief Loads plugins
    \param[in] argc arguments count
    \param[in] argv arguments
    \param[out] params parsed arguments
    \return 0 if success, -1 else
*/
int parse_args(int argc, char *argv[]) {
    const struct option long_opts[] = {
        {"config", required_argument, NULL, 'c'},
        {"logs", required_argument, NULL, 'l'},
        {"plugins", required_argument, NULL, 'p'},
        {NULL, 0, 0, 0}};
    const char *short_opts = "c:l:p:";
    int cur_opt;
    while ((cur_opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) !=
           -1) {
        switch (cur_opt) {
        case ('c'): {
            ConfigVariable variable = {
                "config", NULL, {(long int *)&optarg}, STRING, 1};
            set_variable(variable);
            break;
        }
        case ('l'): {
            ConfigVariable variable = {
                "logs", NULL, {(long int *)&optarg}, STRING, 1};
            set_variable(variable);
            break;
        }
        default: {
            return -1;
        }
        }
    }
    return 0;
}

/*!
    \brief gets config parameters from env
    \return 0 if SUCCESS, -1 else
*/
int parse_envs(void) {
    // get path to config from environment
    char *config_path = getenv("PROXY_CONFIG_PATH");
    if (config_path) {
        ConfigVariable variable = {
            "config", NULL, {(long int *)&config_path}, STRING, 1};
        set_variable(variable);
    }

    // get path to log file from environment
    char *log_path = getenv("PROXY_LOG_PATH");
    if (log_path) {
        ConfigVariable variable = {
            "logs", NULL, {(long int *)&log_path}, STRING, 1};
        set_variable(variable);
    }

    // get plugins from environment
    char *plugins_list_env = getenv("PROXY_MASTER_PLUGINS");
    if (plugins_list_env) {
        char *plugins_list = strdup(plugins_list_env);
        char **plugins = malloc(sizeof(char *));
        int cnt = 0;
        char *plugin = strtok(plugins_list, ",");
        while (plugin) {
            cnt++;
            plugins = realloc(plugins, cnt * sizeof(char *));
            plugins[cnt - 1] = strdup(plugin);
        }
        ConfigVariable variable = {
            "plugins", NULL, {(long int *)plugins}, STRING, cnt};
        set_variable(variable);
        free(plugins_list);
        free(plugins);
    }
    return 0;
};

/*!
    \brief initializes standard values
    \return 0 if success, -1 else
*/
int init_config_values(char *exec_path) {
    // set boot value to path to config
    const char *std_config_path = "../proxy.conf";
    char *config_path = create_path_from_call_dir(exec_path, std_config_path);

    ConfigVariable variable = {
        "config", NULL, {(long int *)&config_path}, STRING, 1};
    set_variable(variable);

    free(config_path);

    // set boot value to log stream
    enum OutputStream log_stream = STDERR;
    long int log_stream_buf = log_stream;

    ConfigVariable log_stream_var = {
        "log_stream", NULL, {&log_stream_buf}, INTEGER, 1};
    set_variable(log_stream_var);

    return 0;
}

/*!
    \brief sets program mode
    \return 0 if success, -1 else
*/
int set_program_mode(char *hardlink) {
    if (!hardlink)
        return -1;

    // get file name
    int start_pos = strlen(hardlink) - 1;
    while (start_pos >= 0 && hardlink[start_pos] != '/') {
        start_pos--;
    }

    // if "debug_proxy", then use stdout for log and set flag for program_mode
    int64_t program_mode = 0;
    if (!strcmp(hardlink + start_pos + 1, "debug_proxy")) {

        enum OutputStream log_stream = STDOUT;
        long int log_stream_buf = log_stream;
        ConfigVariable variable = {
            "log_stream", NULL, {&log_stream_buf}, INTEGER, 1};
        set_variable(variable);

        program_mode = 1;
    }

    ConfigVariable program_mode_var = {
        "program_mode", NULL, {&program_mode}, INTEGER, 1};
    set_variable(program_mode_var);
    return 0;
}

int main(int argc, char **argv) {

    // INITIALIZATION

    struct PluginsStack *plugins = init_plugins_stack(100);

    if (init_logger("./proxy.log", -1)) {
        fprintf(stderr, "Failed to initialize the logger\n");
        goto error_termination;
    }

    if (create_config_table()) {
        fprintf(stderr, "Failed to initialize the config\n");
        goto error_termination;
    }

    if (argc <= 0)
        goto error_termination;

    // set standard values
    init_config_values(argv[0]);

    set_program_mode(argv[0]);

    ConfigVariable log_stream = get_variable("log_stream");

    // set config values from command line
    parse_args(argc, argv);

    // set config variables from env
    parse_envs();

    // read config
    ConfigVariable config_var = get_variable("config");
    parse_config(config_var.data.string[0]);
    destroy_variable(&config_var);

    // load plugins
    ConfigVariable plugin_names = get_variable("plugins");
    load_plugins(plugin_names.data.string, 1, NULL, plugins, argv[0]);
    destroy_variable(&plugin_names);

    // MAIN LOOP

    if (executor_start_hook)
        executor_start_hook();

    close_plugins(plugins);
    free(plugins);
    return 0;

error_termination:
    close_plugins(plugins);
    free(plugins);
    return 1;
}