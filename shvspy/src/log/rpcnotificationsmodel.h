#pragma once

#include <shv/visu/logtablemodelbase.h>

namespace shv::chainpack { class RpcMessage; }

class RpcNotificationsModel : public shv::visu::LogTableModelBase
{
	Q_OBJECT

	using Super = shv::visu::LogTableModelBase;
public:
	enum Columns {ColTimeStamp = 0, ColBroker, ColShvPath, ColSource, ColSignal, ColParams, ColCnt};
public:
	RpcNotificationsModel(QObject *parent = nullptr);

	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	int columnCount(const QModelIndex &) const override {return ColCnt;}

	void addLogRow(const std::string &broker_name, const shv::chainpack::RpcMessage &msg);
};
