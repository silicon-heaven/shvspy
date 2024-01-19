#include "texteditdialog.h"
#include "ui_texteditdialog.h"

#include <shv/chainpack/rpcvalue.h>

#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTimer>

namespace cp = shv::chainpack;

//=========================================================
// TextEditDialog
//=========================================================
TextEditDialog::TextEditDialog(QWidget *parent)
	: Super(parent)
	, ui(new Ui::TextEditDialog)
{
	ui->setupUi(this);
	ui->lblError->hide();
	ui->btFormatCpon->hide();
	ui->btCompactCpon->hide();
	ui->btSaveToFile->hide();
	setReadOnly(false);

	QSettings settings;
	restoreGeometry(settings.value(QStringLiteral("ui/ResultView/geometry")).toByteArray());

	ui->plainTextEdit->setFocus();
	ui->searchWidget->hide();
	ui->plainTextEdit->installEventFilter(this);
	ui->searchEdit->installEventFilter(this);
	installEventFilter(this);
	connect(ui->closeToolButton, &QToolButton::clicked, ui->searchWidget, &QWidget::hide);
	connect(ui->nextToolButton, &QToolButton::clicked, this, &TextEditDialog::search);
	connect(ui->prevToolButton, &QToolButton::clicked, this, &TextEditDialog::searchBack);
	connect(ui->btSaveToFile, &QPushButton::clicked, this, &TextEditDialog::saveToFile);
}

TextEditDialog::~TextEditDialog()
{
	QSettings settings;
	settings.setValue(QStringLiteral("ui/ResultView/geometry"), saveGeometry());
	delete ui;
}

void TextEditDialog::setText(const QString &s)
{
	ui->plainTextEdit->setPlainText(s);
}

void TextEditDialog::setBlob(const QByteArray &s)
{
	m_blobData = s;
	ui->plainTextEdit->setPlainText(QString::fromUtf8(s));
}

QString TextEditDialog::text() const
{
	return ui->plainTextEdit->toPlainText();
}

void TextEditDialog::setReadOnly(bool ro)
{
	ui->plainTextEdit->setReadOnly(ro);
	ui->btSave->setVisible(!ro);
	ui->btSaveToFile->setVisible(ro);
}

bool TextEditDialog::eventFilter(QObject *o, QEvent *e)
{
	if (e->type() == QEvent::KeyPress) {
		auto *ke = static_cast<QKeyEvent *>(e);
		if (o == ui->plainTextEdit || o == this) {
			if ((ke->key() == Qt::Key_F && ke->modifiers() == Qt::CTRL) ||
				(ke->key() == Qt::Key_Slash && ke->modifiers() == Qt::NoModifier && ui->plainTextEdit->isReadOnly())) {
				ui->searchWidget->show();
				ui->searchEdit->setFocus();
				return true;
			}
		}
		else if (o == ui->searchEdit) {
			if ((ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return) && ke->modifiers() == Qt::NoModifier) {
				if (ui->searchEdit->isModified()) {
					ui->plainTextEdit->moveCursor(QTextCursor::MoveOperation::Start);
					ui->searchEdit->setModified(false);
				}
				search();
				return true;
			}
			else if (ke->key() == Qt::Key_Escape && ke->modifiers() == Qt::NoModifier) {
				ui->searchWidget->hide();
				return true;
			}
			else if (ke->key() == Qt::Key_F3) {
				if (ke->modifiers() == Qt::NoModifier) {
					search();
					return true;
				}
				else if (ke->modifiers() == Qt::SHIFT) {
					searchBack();
					return true;
				}
			}
		}
	}
	return Super::eventFilter(o, e);
}

void TextEditDialog::search()
{
	ui->plainTextEdit->find(ui->searchEdit->text());
}

void TextEditDialog::searchBack()
{
	ui->plainTextEdit->find(ui->searchEdit->text(), QTextDocument::FindFlag::FindBackward);
}

void TextEditDialog::saveToFile()
{
	QString file_name = QFileDialog::getSaveFileName(this, tr("Save to file"));
	if (!file_name.isEmpty()) {
		QFile f(file_name);
		if (!f.open(QFile::WriteOnly)) {
			QMessageBox::warning(this, tr("Warning"), tr("Cannot open file ") + file_name);
			return;
		}
		if (!m_blobData.isEmpty()) {
			f.write(m_blobData);
		}
		else {
			f.write(text().toUtf8());
		}
	}
}

//=========================================================
// CponEditDialog
//=========================================================
CponEditDialog::CponEditDialog(QWidget *parent)
	: Super(parent)
{
	ui->btFormatCpon->show();
	ui->btCompactCpon->show();

	connect(ui->plainTextEdit, &QPlainTextEdit::textChanged, this, &CponEditDialog::validateContentDeferred);
	connect(ui->btCompactCpon, &QPushButton::clicked, this, &CponEditDialog::onBtCompactCponClicked);
	connect(ui->btFormatCpon, &QPushButton::clicked, this, &CponEditDialog::onBtFormatCponClicked);
}

void CponEditDialog::setValidateContent(bool b)
{
	m_isValidateContent = b;
}

void CponEditDialog::validateContentDeferred()
{
	if(!m_isValidateContent)
		return;
	if(!m_validateTimer) {
		m_validateTimer = new QTimer(this);
		m_validateTimer->setSingleShot(true);
		m_validateTimer->setInterval(1000);
		connect(m_validateTimer, &QTimer::timeout, this, &CponEditDialog::validateContent);
	}
	m_validateTimer->start();
}

cp::RpcValue CponEditDialog::validateContent()
{
	QString txt = text().trimmed();
	std::string err;
	cp::RpcValue rv = cp::RpcValue::fromCpon(txt.toStdString(), &err);
	if(txt.isEmpty() || err.empty()) {
		ui->lblError->setVisible(false);
		return rv;
	}
	else {
		ui->lblError->setText(tr("Malformed Cpon: ") + QString::fromStdString(err));
		ui->lblError->setVisible(true);
		return cp::RpcValue();
	}
}

void CponEditDialog::onBtCompactCponClicked()
{
	shv::chainpack::RpcValue rv = validateContent();
	if(rv.isValid()) {
		std::string cpon = rv.toCpon();
		setText(QString::fromStdString(cpon));
	}
}

void CponEditDialog::onBtFormatCponClicked()
{
	shv::chainpack::RpcValue rv = validateContent();
	if(rv.isValid()) {
		std::string cpon = rv.toCpon("  ");
		setText(QString::fromStdString(cpon));
	}
}

