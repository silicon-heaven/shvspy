#pragma once

#include <shv/chainpack/rpcvalue.h>

#include <QAbstractTableModel>
#include <QPointer>


class ShvNodeItem;
struct ShvMetaMethod;

namespace shv { namespace chainpack { class RpcMessage; }}

class AttributesModel : public QAbstractTableModel
{
	Q_OBJECT
private:
	typedef QAbstractTableModel Super;
public:
	enum Columns {ColMethodName = 0, ColParamType, ColResultType, ColFlags, ColAccessLevel, ColParams, ColResult, ColBtRun, ColError, ColAttributes, ColCnt};
	enum Roles {RpcValueRole = Qt::UserRole };
public:
	AttributesModel(QObject *parent = nullptr);
	~AttributesModel() override = default;
public:
	int rowCount(const QModelIndex &parent) const override;
	int columnCount(const QModelIndex &parent) const override {Q_UNUSED(parent) return ColBtRun + 1;}
	Qt::ItemFlags flags(const QModelIndex &ix) const Q_DECL_OVERRIDE;
	QVariant data( const QModelIndex & index, int role = Qt::DisplayRole ) const Q_DECL_OVERRIDE;
	bool setData(const QModelIndex &ix, const QVariant &val, int role = Qt::EditRole) Q_DECL_OVERRIDE;
	QVariant headerData ( int section, Qt::Orientation o, int role = Qt::DisplayRole ) const Q_DECL_OVERRIDE;

	void load(ShvNodeItem *nd);
	void callMethod(unsigned row, bool throw_exc = false);

	QString path() const;
	QString method(int row) const;

	Q_SIGNAL void reloaded();
	Q_SIGNAL void methodCallResultChanged(int method_ix);
private:
	void onMethodsLoaded();
	void onRpcMethodCallFinished(int method_ix);
	const ShvMetaMethod *metaMethodAt(unsigned method_ix);
	void loadRow(unsigned method_ix);
	void loadRows();
	void emitRowChanged(int row_ix);
	void callGetters();
private:
	QPointer<ShvNodeItem> m_shvTreeNodeItem;
	using RowVals = shv::chainpack::RpcValue::List;
	std::vector<RowVals> m_rows;
};
