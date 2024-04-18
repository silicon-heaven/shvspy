#ifndef DLGCALLSHVMETHOD_H
#define DLGCALLSHVMETHOD_H

#include <QDialog>

namespace Ui {
class DlgCallShvMethod;
}

namespace shv::iotqt::rpc { class ClientConnection; }

class DlgCallShvMethod : public QDialog
{
	Q_OBJECT

public:
	explicit DlgCallShvMethod(shv::iotqt::rpc::ClientConnection *connection, QWidget *parent = nullptr);
	~DlgCallShvMethod() override;

	void setShvPath(const std::string &path);
private:
	void callShvMethod();
private:
	Ui::DlgCallShvMethod *ui;
	shv::iotqt::rpc::ClientConnection *m_connection;
};

#endif // DLGCALLSHVMETHOD_H
