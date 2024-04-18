#pragma once

#include <QDialog>

namespace shv::chainpack { class RpcValue; }

namespace Ui {
class TextEditDialog;
}

class TextEditDialog : public QDialog
{
	Q_OBJECT

	using Super = QDialog;
public:
	explicit TextEditDialog(QWidget *parent = nullptr);
	~TextEditDialog() override;

	void setText(const QString &s);
	void setBlob(const QByteArray &s);
	QString text() const;

	void setReadOnly(bool ro);
protected:
	bool eventFilter(QObject *o, QEvent *e) override;
	void search();
	void searchBack();
	void saveToFile();

	Ui::TextEditDialog *ui;
	QByteArray m_blobData;
};

class CponEditDialog : public TextEditDialog
{
	Q_OBJECT

	using Super = TextEditDialog;
public:
	explicit CponEditDialog(QWidget *parent = nullptr);

	void setValidateContent(bool b);
private slots:
	void onBtCompactCponClicked();
	void onBtFormatCponClicked();
private:
	void validateContentDeferred();
	shv::chainpack::RpcValue validateContent();
private:
	bool m_isValidateContent = false;
	QTimer *m_validateTimer = nullptr;
};
