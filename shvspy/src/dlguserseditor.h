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
	explicit DlgUsersEditor(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection);
	~DlgUsersEditor();
	void init(const std::string &path);

private:
	Ui::DlgUsersEditor *ui;

	void listUsers();
	QString selectedUser();

	void onAddUserClicked();
	void onDelUserClicked();
	void onEditUserClicked();
	void onTableUsersDoubleClicked(QModelIndex ix);

	std::string aclEtcUsersNodePath();
	void setFilter(const QString &filter);

	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	std::string m_aclEtcNodePath;
	QStandardItemModel *m_dataModel;
	QSortFilterProxyModel *m_modelProxy;
};

#endif // DLGUSERSEDITOR_H
