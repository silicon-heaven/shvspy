#ifndef DLGADDEDITMOUNT_H
#define DLGADDEDITMOUNT_H

#include <shv/iotqt/acl/aclmountdef.h>

#include <QDialog>
#include <QTableView>

namespace Ui {
class DlgAddEditMount;
}

namespace shv::iotqt::rpc { class ClientConnection; }

class DlgAddEditMount : public QDialog
{
	Q_OBJECT

public:
	enum class DialogType {Add = 0, Edit, Count};

	explicit DlgAddEditMount(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_node_path, DlgAddEditMount::DialogType dt = DialogType::Add);
	~DlgAddEditMount() override;

	DialogType dialogType();
	void init(const QString &mount_name);
	void accept() Q_DECL_OVERRIDE;

private:
	void callSetMountSettings();
	void callGetMountSettings();
	void checkExistingMountId(std::function<void(bool, bool)> callback);

	std::string aclEtcMountNodePath();
	std::string mountShvPath();

	void setStatusText(const QString &txt);
private:
	Ui::DlgAddEditMount *ui;
	DialogType m_dialogType;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	std::string m_aclEtcNodePath;
	shv::iotqt::acl::AclMountDef m_mount;
};

#endif // DLGADDEDITMOUNT_H
