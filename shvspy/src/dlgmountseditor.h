#pragma once

#include <QDialog>
#include <QMap>

#include <shv/iotqt/rpc/clientconnection.h>

namespace Ui {
class DlgMountsEditor;
}

class QStandardItemModel;
class QSortFilterProxyModel;

class DlgMountsEditor : public QDialog
{
	Q_OBJECT

public:
	explicit DlgMountsEditor(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &broker_path, const shv::chainpack::IRpcConnection::ShvApiVersion brokerApiVersion);
	~DlgMountsEditor() override;
private:
	std::string aclEtcMountsNodePath();

	QString selectedMount();
	void listMounts();
	void getMountPointDefinition(const QString &id);

	void onAddMountClicked();
	void onDeleteMountClicked();
	void onEditMountClicked();
	void onTableMountDoubleClicked(QModelIndex ix);
	void onRpcCallsFinished();

	void setStatusText(const QString &txt);
	void checkRpcCallsFinished();

	enum RpcCallStatus { Unfinished, Ok, Error };
	struct MountPointInfo {
		QString id;
		QString mountPoint;
		QString description;
		RpcCallStatus status = Unfinished;
	};
	Ui::DlgMountsEditor *ui;
	std::string m_aclNodePath;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	QMap<QString, MountPointInfo> m_mountPoints;
	QString m_lastCurrentId;
	QStandardItemModel *m_dataModel;
	QSortFilterProxyModel *m_modelProxy;
	shv::chainpack::IRpcConnection::ShvApiVersion m_brokerApiVersion;
};

