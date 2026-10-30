#pragma once

#include "texteditdialog.h"

#include <shv/chainpack/rpcvalue.h>

namespace shv::core::utils { class ShvTypeInfo; }

class ValueInspectorDialog : public CponEditDialog
{
	Q_OBJECT
	using Super = CponEditDialog;

public:
	explicit ValueInspectorDialog(QWidget *parent = nullptr);

	void setResult(const QModelIndex &ix);

private:
	void setTypeInfoNotAvailable(const QString &reason);
	void onTypeInfoLoaded(const shv::core::utils::ShvTypeInfo &type_info, const QString &property_path, const QString &error);
	void showTypeInfo(bool on);

	shv::chainpack::RpcValue m_value;
};
