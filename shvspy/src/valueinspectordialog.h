#pragma once

#include "rpctypedescription.h"
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
	void typeInfoFailed(const QString &reason);
	void setErrorMessage(const QString &reason);
	void showDecodedValue(const DecodedValue &decoded, const QString &source = {});
	bool decodeInlineType();
	void onTypeInfoLoaded(const shv::core::utils::ShvTypeInfo &type_info, const QString &property_path, const QString &error);
	void showTypeInfo(bool on);

	shv::chainpack::RpcValue m_value;
	QString m_inlineResultType;
};
