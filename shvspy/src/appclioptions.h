#pragma once

#include <shv/core/utils/clioptions.h>

#include <QSet>

class AppCliOptions : public shv::core::utils::CLIOptions
{
private:
	using Super = shv::core::utils::CLIOptions;

	CLIOPTION_GETTER_SETTER2(bool, "rpc.metaTypeExplicit", is, set, MetaTypeExplicit)
	CLIOPTION_GETTER_SETTER2(bool, "rawRpcMessageLog", is, set, RawRpcMessageLog)
	CLIOPTION_GETTER_SETTER2(std::string, "configDir", c, setC, onfigDir)
public:
	AppCliOptions();
	//~AppCliOptions() Q_DECL_OVERRIDE {}
};

