#ifndef DLGUSERSEDITOR_H
#define DLGUSERSEDITOR_H

#include <QDialog>

#include <shv/chainpack/rpcvalue.h>

namespace Ui {
class DlgUsersEditor;
}

class QStandardItemModel;
class QSortFilterProxyModel;

namespace shv::iotqt::rpc { class ClientConnection; }

class DlgUsersEditor : public QDialog
{
	Q_OBJECT

public:
	explicit DlgUsersEditor(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &broker_path);
	~DlgUsersEditor() override;

private:
	Ui::DlgUsersEditor *ui;

	void init();
	void listUsers();
	QString selectedUser();

	void onAddUserClicked();
	void onDelUserClicked();
	void onEditUserClicked();
	void onTableUsersDoubleClicked(QModelIndex ix);

	std::string aclAccessPath();
	std::string aclAccessUsersPath();
	void setFilter(const QString &filter);
private:
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	QStandardItemModel *m_dataModel;
	QSortFilterProxyModel *m_modelProxy;
	std::string m_brokerPath;
};

#endif // DLGUSERSEDITOR_H
