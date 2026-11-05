#include "typedescriptiondialog.h"
#include "ui_typedescriptiondialog.h"

#include "rpctypedescription.h"

#include <QFontDatabase>

TypeDescriptionDialog::TypeDescriptionDialog(const QString &type_description, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::TypeDescriptionDialog)
{
	ui->setupUi(this);
	ui->formattedTypeEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	ui->readableTypeEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

	QString parse_error;
	const auto parsed_type = RpcTypeDescription::parse(type_description, &parse_error);
	QString formatted_text = type_description;
	QString readable_text = tr("Could not parse this SHV RPC type description: %1").arg(parse_error);
	if (parsed_type) {
		formatted_text = parsed_type->format("  ");
		readable_text = parsed_type->describe(0);
	}
	ui->formattedTypeEdit->setPlainText(formatted_text);
	ui->readableTypeEdit->setPlainText(readable_text);
}

TypeDescriptionDialog::~TypeDescriptionDialog()
{
	delete ui;
}
