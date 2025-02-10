#include "../../include/master.h"
#include "../../include/config.h"
#include "../../include/logger.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*HOOKS DEFINITIONS*/
Hook executor_start_hook = NULL;
Hook executor_end_hook = NULL;

/*STRUCTS*/

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

/*FUNCTION DECLARATIONS*/
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

/*FUNCTION DEFINITIONS*/

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
    void (*init_plugin)(void);
    ///< this function will be executed for each contrib library
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
            return -1;
        }

        init_plugin();
    }
    return 0;
}

int main(int argc, char **argv) {
    struct PluginsStack *plugins = init_plugins_stack(100);

    if (init_logger(NULL, -1)) {
        fprintf(stderr, "Failed to initialize the logger\n");
        goto error_termination;
    }

    if (create_config_table()) {
        fprintf(stderr, "Failed to initialize the config\n");
        goto error_termination;
    }

    if (argc > 0)
        printf("%s\n", argv[0]);
    else
        goto error_termination;

    char *greeting_name = "greeting";
    char **plugin_names = &greeting_name;


    if (load_plugins(plugin_names, 1, NULL, plugins, argv[0]))
        goto error_termination;

    if (executor_start_hook)
        executor_start_hook();


    close_plugins(plugins);
    free(plugins);
    return 0;

error_termination:
    close_plugins(plugins);
    free(plugins);
    return -1;
}