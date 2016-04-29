#include "gcc-plugin.h"
#include "plugin-version.h"

int plugin_is_GPL_compatible = 1;

static void
_start_parse(void *gcc_data, void *user_data)
{
	printf("Declaration parsed\n");
}

static struct plugin_info _plugin_info = {
	.version = "Initial",
	.help = "This is the help"
};

int plugin_init(struct plugin_name_args *plugin_info,
		struct plugin_gcc_version *version)
{
	if (!plugin_default_version_check (version, &gcc_version)) {
		return 1;
	}

	register_callback(plugin_info->base_name,
			  PLUGIN_INFO, NULL, &_plugin_info);

	register_callback(plugin_info->base_name,
			  PLUGIN_FINISH_DECL,
			  _start_parse, NULL);

	return 0;
}
