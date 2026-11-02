#pragma once

#include <shv/chainpack/rpcvalue.h>

#include <QAbstractTableModel>
#include <QPointer>


class ShvNodeItem;
struct ShvMetaMethod;

namespace shv::chainpack { class RpcResponse; }
namespace shv::iotqt::rpc { class ClientConnection; }

class AttributesModel : public QAbstractTableModel
{
	Q_OBJECT
private:
	typedef QAbstractTableModel Super;
public:
	enum Columns {ColMethodName = 0, ColParamType, ColResultType, ColSignals, ColFlags, ColAccessLevel, ColParams, ColResult, ColBtRun, ColError, ColMetaMethod, ColCnt};
public:
	AttributesModel(QObject *parent = nullptr);
	~AttributesModel() override = default;
public:
	int rowCount(const QModelIndex &parent) const override;
	int columnCount(const QModelIndex &parent = {}) const override { Q_UNUSED(parent); return ColBtRun + 1; }
	Qt::ItemFlags flags(const QModelIndex &ix) const Q_DECL_OVERRIDE;
	QVariant data( const QModelIndex & index, int role = Qt::DisplayRole ) const Q_DECL_OVERRIDE;
	bool setData(const QModelIndex &ix, const QVariant &val, int role = Qt::EditRole) Q_DECL_OVERRIDE;
	QVariant headerData ( int section, Qt::Orientation o, int role = Qt::DisplayRole ) const Q_DECL_OVERRIDE;

	void load(ShvNodeItem *nd);
	void callMethod(unsigned row, bool throw_exc = false);

	QString path() const;
	QString methodName(int row) const;
	shv::chainpack::RpcValue methodParams(int row) const;
	shv::chainpack::RpcResponse methodCallResult(int row) const;
	shv::iotqt::rpc::ClientConnection *clientConnection() const;

private:
	void onMethodsLoaded();
	void onRpcMethodCallFinished(int method_ix);
	void callGetters();

	QPointer<ShvNodeItem> m_shvTreeNodeItem;
};
