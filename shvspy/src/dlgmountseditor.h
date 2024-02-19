#pragma once

#include <QDialog>
#include <QMap>

namespace Ui {
class DlgMountsEditor;
}

namespace shv::iotqt::rpc { class ClientConnection; }

class QStandardItemModel;
class QSortFilterProxyModel;

class DlgMountsEditor : public QDialog
{
	Q_OBJECT

public:
	explicit DlgMountsEditor(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection);
	~DlgMountsEditor();
	void init(const std::string &acl_node_path);

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
	std::string m_aclEtcNodePath;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	QMap<QString, MountPointInfo> m_mountPoints;
	QString m_lastCurrentId;
	QStandardItemModel *m_dataModel;
	QSortFilterProxyModel *m_modelProxy;
};

