#include "appclioptions.h"

namespace cp = shv::chainpack;

AppCliOptions::AppCliOptions()
{
	addOption("rawRpcMessageLog").setType(cp::RpcValue::Type::Bool)
			.setNames("--rmsg", "--raw-rpcmessage-log")
			.setComment("RpcMessage is logged as is without type ids translated to text")
			.setDefaultValue(false);
	addOption("rpc.metaTypeExplicit").setType(cp::RpcValue::Type::Bool).setNames("--mtid", "--rpc-metatype-explicit").setComment("RpcMessage Type ID is included in RpcMessage when set, for more verbose -v rpcmsg log output").setDefaultValue(false);
	addOption("configDir").setType(cp::RpcValue::Type::String).setNames("--config-dir")
			.setComment("Application config dir, default value is '$HOME/.config', application will look for config file 'Elektroline/shvspy.conf' there.");
	addOption("requestTimeout").setType(cp::RpcValue::Type::Int).setNames("--rqt", "--request-timeout")
			.setDefaultValue(5000)
			.setComment("RPC request timeout in milliseconds.");
}
