#pragma once

#include <QDialog>
#include <QString>

namespace Ui {
class TypeDescriptionDialog;
}

class TypeDescriptionDialog : public QDialog
{
	Q_OBJECT

public:
	explicit TypeDescriptionDialog(const QString &type_description, QWidget *parent = nullptr);
	~TypeDescriptionDialog() override;

private:
	Ui::TypeDescriptionDialog *ui;
};
