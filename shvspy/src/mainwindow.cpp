#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "theapp.h"
#include "attributesmodel/attributesmodel.h"
#include "servertreemodel/servertreemodel.h"
#include "servertreemodel/shvbrokernodeitem.h"
#include "log/rpcnotificationsmodel.h"
#include "dlgbrokerproperties.h"
//#include "dlgsubscriptionparameters.h"
#include "dlgcallshvmethod.h"
#include "dlguserseditor.h"
#include "dlgroleseditor.h"
#include "dlgmountseditor.h"
#include "methodparametersdialog.h"
#include "texteditdialog.h"

#include <shv/chainpack/chainpackreader.h>
#include <shv/chainpack/chainpackwriter.h>
#include <shv/chainpack/cponreader.h>
#include <shv/chainpack/cponwriter.h>
#include <shv/visu/logview/dlgloginspector.h>
#include <shv/visu/errorlogmodel.h>
#include <shv/visu/logwidget.h>

#include <shv/coreqt/log.h>

#include <shv/coreqt/rpc.h>
#include <shv/core/utils/shvpath.h>

#include <QSettings>
#include <QMessageBox>
#include <QItemSelectionModel>
#include <QInputDialog>
#include <QScrollBar>
#include <QFileDialog>

#include <fstream>

namespace cp = shv::chainpack;

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	addAction(ui->actionQuit);
	connect(ui->actionQuit, &QAction::triggered, TheApp::instance(), &TheApp::quit);
	//setWindowTitle(tr("QFreeOpcUa Spy"));
	setWindowIcon(QIcon(":/shvspy/images/shvspy"));

	ui->menu_View->addAction(ui->dockServers->toggleViewAction());
	ui->menu_View->addAction(ui->dockAttributes->toggleViewAction());
	ui->menu_View->addAction(ui->dockNotifications->toggleViewAction());
	ui->menu_View->addAction(ui->dockErrors->toggleViewAction());
	ui->menu_View->addAction(ui->dockSubscriptions->toggleViewAction());

	ServerTreeModel *tree_model = TheApp::instance()->serverTreeModel();
	ui->treeServers->setModel(tree_model);
	connect(tree_model, &ServerTreeModel::dataChanged, ui->treeServers,[this](const QModelIndex &tl, const QModelIndex &br, const QVector<int> &roles) {
		/// expand broker node when children loaded
		Q_UNUSED(roles)
		if(tl == br) {
			ServerTreeModel *tree_model = TheApp::instance()->serverTreeModel();
			auto *brit = qobject_cast<ShvBrokerNodeItem*>(tree_model->itemFromIndex(tl));
			if(brit) {
				if(tree_model->hasChildren(tl)) {
					ui->treeServers->expand(tl);
				}
			}
		}
	});
	connect(ui->treeServers->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::onShvTreeViewCurrentSelectionChanged);
	connect(tree_model, &ServerTreeModel::brokerConnectedChanged, ui->subscriptionsWidget, &SubscriptionsWidget::onBrokerConnectedChanged);
	connect(tree_model, &ServerTreeModel::subscriptionAdded, ui->subscriptionsWidget, &SubscriptionsWidget::onSubscriptionAdded);
	connect(tree_model, &ServerTreeModel::brokerLoginError, this, [this](int broker_id, const QString &error_message, int err_cnt) {
		Q_UNUSED(broker_id);
		TheApp::instance()->errorLogModel()->addLogRow(NecroLogLevel::Error, error_message.toStdString(), err_cnt);
		if(err_cnt == 1)
			QMessageBox::critical(this, tr("Login error"), error_message);
	});
	connect(tree_model, &ServerTreeModel::subscriptionAddError, this, [this](int broker_id, const std::string &shv_path, const std::string &error_msg) {
		QString msg = tr("Add subscription error:") + " " + QString::fromStdString(error_msg) + " " + QString::number(broker_id) + " " + tr("shv path:") + " " + QString::fromStdString(shv_path);
		QMessageBox::critical(this, tr("Subsription error"), msg);
		TheApp::instance()->errorLogModel()->addLogRow(NecroLogLevel::Error, msg.toStdString(), broker_id);
	});

	AttributesModel *attr_model = TheApp::instance()->attributesModel();
	ui->tblAttributes->setModel(attr_model);
	ui->tblAttributes->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
	ui->tblAttributes->verticalHeader()->setDefaultSectionSize(static_cast<int>(fontMetrics().height() * 1.3));
	ui->tblAttributes->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(attr_model, &AttributesModel::reloaded, this, [this]() {
		ui->btLogInspector->setEnabled(false);
		ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ui->treeServers->currentIndex());
		if(nd) {
			for(const auto &mm : nd->methods()) {
				if(mm.metamethod.name() == cp::Rpc::METH_GET_LOG) {
					ui->btLogInspector->setEnabled(true);
					break;
				}
			}
		}
	});

	connect(attr_model, &AttributesModel::reloaded, this, &MainWindow::resizeAttributesViewSectionsToFit);
	connect(TheApp::instance()->attributesModel(), &AttributesModel::methodCallResultChanged, this, [this](int method_ix) {
		Q_UNUSED(method_ix)
		this->resizeAttributesViewSectionsToFit();
	});
	connect(ui->tblAttributes, &QTableView::customContextMenuRequested, this, &MainWindow::onAttributesTableContextMenu);

	connect(ui->tblAttributes, &QTableView::activated, this, [this](const QModelIndex &ix) {
		if(ix.column() == AttributesModel::ColBtRun) {
			try {
				TheApp::instance()->attributesModel()->callMethod(ix.row(), shv::core::Exception::Throw);
			}  catch (const std::exception &e) {
				QMessageBox::warning(this, tr("Method call error"), tr("Method call error: %1").arg(e.what()));
			}
		}
	});
	connect(ui->tblAttributes, &QTableView::doubleClicked, this, [this](const QModelIndex &ix) {
		if (ix.column() == AttributesModel::ColResult) {
			displayResult(ix);
		}
		else if (ix.column() == AttributesModel::ColParams) {
			editCponParameters(ix);
		}
	}, Qt::QueuedConnection);

	connect(ui->btLogInspector, &QPushButton::clicked, this, &MainWindow::openLogInspector);

	ui->notificationsLogWidget->setLogTableModel(TheApp::instance()->rpcNotificationsModel());
	connect(ui->notificationsLogWidget->tableView(), &QTableView::doubleClicked, this, &MainWindow::onNotificationsDoubleClicked);
	ui->errorLogWidget->setLogTableModel(TheApp::instance()->errorLogModel());

	checkSettingsReady();

	connect(ui->edAttributesShvPath, &QLineEdit::returnPressed, ui->btGotoShvPath, &QPushButton::click);
	connect(ui->btGotoShvPath, &QPushButton::clicked, this, &MainWindow::gotoShvPath);
	connect(ui->actAddServer, &QAction::triggered, this, &MainWindow::onActAddServer_triggered);
	connect(ui->actEditServer, &QAction::triggered, this, &MainWindow::onActEditServer_triggered);
	connect(ui->actCopyServer, &QAction::triggered, this, &MainWindow::onActCopyServer_triggered);
	connect(ui->actRemoveServer, &QAction::triggered, this, &MainWindow::onActRemoveServer_triggered);
	connect(ui->actHelpAbout, &QAction::triggered, this, &MainWindow::onActHelpAbout_triggered);

	connect(ui->treeServers, &ServerTreeView::doubleClicked, this, &MainWindow::onTreeServers_doubleClicked);
	connect(ui->treeServers, &ServerTreeView::enterKeyPressed, this, &MainWindow::onTreeServers_enterKeyPressed);
	connect(ui->treeServers, &ServerTreeView::customContextMenuRequested, this, &MainWindow::onTreeServers_customContextMenuRequested);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::checkSettingsReady()
{
#ifdef Q_OS_WASM
	// WASM settings will be ready at some later point in time - when
	// QSettings::status() returns NoError.
	// see https://github.com/msorvig/qt-webassembly-examples/tree/master/gui_settings
	shvInfo() << "Waiting for settings initialized";
	if (m_settings.status() != QSettings::NoError) {
		QTimer::singleShot(2000, this, &MainWindow::checkSettingsReady);
		return;
	}
	shvInfo() << "Settings initialized OK";
#endif
	auto default_config = []() {
		static bool is_loaded = false;
		static shv::chainpack::RpcValue config;
		if(!is_loaded) {
			is_loaded = true;
			QFile f(":/shvspy/config/default-config.json");
			if(f.open(QFile::ReadOnly)) {
				QByteArray ba = f.readAll();
				std::string cpon = ba.toStdString();
				shvInfo() << "Loading resources setting file:" << f.fileName() << ":\n" << cpon;
				std::string err;
				config = shv::chainpack::RpcValue::fromCpon(cpon, &err);
				if(!err.empty()) {
					shvError() << "Erorr parse config file:" << err;
				}
			}
			else {
				shvWarning() << "Cannot read config file:" << f.fileName();
			}
		}
		return config;
	};
	QString servers_json = m_settings.value("application/servers").toString();
	if(servers_json.isEmpty()) {
		const shv::chainpack::RpcValue rv = default_config();
		const shv::chainpack::RpcValue &m = rv.asMap().value("application").asMap().value("servers");
		TheApp::instance()->serverTreeModel()->loadSettings(m);
	}
	else {
		TheApp::instance()->loadSettings(m_settings);
	}
	restoreGeometry(m_settings.value(QStringLiteral("ui/mainWindow/geometry")).toByteArray());
	QByteArray wstate = m_settings.value(QStringLiteral("ui/mainWindow/state")).toByteArray();
	if(wstate.isEmpty()) {
		const shv::chainpack::RpcValue rv = default_config();
		const std::string &s = rv.asMap().valref("ui")
				.asMap().valref("mainWindow")
				.asMap().valref("state").asString();
		//shvInfo() << "default wstate:" << s;
		auto ba = QByteArray::fromStdString(s);
		//shvInfo() << "default wstat2:" << ba.toStdString();
		wstate = QByteArray::fromHex(ba);
		//shvInfo() << "default wstat3:" << wstate.toStdString();
	}
	//shvInfo() << "restoring wstate:" << wstate.toHex().toStdString();
	restoreState(wstate);
}

void MainWindow::resizeAttributesViewSectionsToFit()
{
	QHeaderView *hh = ui->tblAttributes->horizontalHeader();
	hh->resizeSections(QHeaderView::ResizeToContents);
	int sum_section_w = 0;
	for (int i = 0; i < hh->count(); ++i)
		sum_section_w += hh->sectionSize(i);

	int widget_w = ui->tblAttributes->geometry().size().width();
	if(sum_section_w - widget_w > 0) {
		int w_params = hh->sectionSize(AttributesModel::ColParams);
		int w_result = hh->sectionSize(AttributesModel::ColResult);
		int w_run = hh->sectionSize(AttributesModel::ColBtRun);
		int w_section_rest = sum_section_w - w_params - w_result + w_run;
		int w_params2 = w_params * (widget_w - w_section_rest) / (w_params + w_result);
		int w_result2 = w_result * (widget_w - w_section_rest) / (w_params + w_result);
		//shvDebug() << "widget:" << widget_w << "com col w:" << sum_section_w << "params section size:" << w_params << "result section size:" << w_result;
		hh->resizeSection(AttributesModel::ColParams, w_params2);
		hh->resizeSection(AttributesModel::ColResult, w_result2);
	}
}

void MainWindow::onActAddServer_triggered()
{
	editServer(nullptr, false);
}

void MainWindow::onActEditServer_triggered()
{
	QModelIndex ix = ui->treeServers->currentIndex();
	ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ix);
	auto *brnd = qobject_cast<ShvBrokerNodeItem*>(nd);
	if(brnd) {
		editServer(brnd, false);
	}
}

void MainWindow::onActCopyServer_triggered()
{
	QModelIndex ix = ui->treeServers->currentIndex();
	ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ix);
	auto *brnd = qobject_cast<ShvBrokerNodeItem*>(nd);
	if(brnd) {
		editServer(brnd, true);
	}
}

void MainWindow::onActRemoveServer_triggered()
{
	QModelIndex ix = ui->treeServers->currentIndex();
	ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ix);
	auto *brnd = qobject_cast<ShvBrokerNodeItem*>(nd);
	if(brnd) {
		auto *box = new QMessageBox(
					QMessageBox::Question,
					tr("Question"),
					tr("Realy drop server definition for '%1'").arg(nd->objectName())
					, QMessageBox::Yes | QMessageBox::No
					, this);
		connect(box, &QMessageBox::buttonClicked, this, [=](QAbstractButton *button) {
			if(button == box->button(QMessageBox::Yes))
				TheApp::instance()->serverTreeModel()->invisibleRootItem()->deleteChild(ix.row());
			box->deleteLater();
		});
		box->show();
		box->adjustSize();
	}
}

void MainWindow::onTreeServers_customContextMenuRequested(const QPoint &pos)
{
	QModelIndex ix = ui->treeServers->indexAt(pos);
	ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ix);
	auto *snd = qobject_cast<ShvBrokerNodeItem*>(nd);
	auto *m = new QMenu();
	auto *a_reloadNode = new QAction(tr("Reload"), m);
	auto *a_subscribeNode = new QAction(tr("Subscribe"), m);
	auto *a_callShvMethod = new QAction(tr("Call shv method"), m);
	auto *a_usersEditor = new QAction(tr("Users editor"), m);
	auto *a_rolesEditor = new QAction(tr("Roles editor"), m);
	auto *a_mountsEditor = new QAction(tr("Mounts editor"), m);

	//QAction *a_test = new QAction(tr("create test.txt"), &m);
	if(!nd) {
		m->addAction(ui->actAddServer);
	}
	else if(snd) {
		m->addAction(ui->actAddServer);
		m->addAction(ui->actEditServer);
		m->addAction(ui->actCopyServer);
		m->addAction(ui->actRemoveServer);
		if(snd->isOpen()) {
			m->addSeparator();
			m->addAction(a_reloadNode);
		}
	}
	else {
		m->addAction(a_reloadNode);
		m->addAction(a_subscribeNode);
		m->addAction(a_callShvMethod);

		if (nd->nodeId() == ".broker"){
			m->addAction(a_usersEditor);
			m->addAction(a_rolesEditor);
			m->addAction(a_mountsEditor);
		}
	}
	if(m->actions().isEmpty()) {
		delete m;
	}
	else {
		m->popup(ui->treeServers->viewport()->mapToGlobal(pos));
		connect(m, &QMenu::triggered, this, [this, a_reloadNode, a_subscribeNode, a_callShvMethod, a_usersEditor, a_rolesEditor, a_mountsEditor, m](QAction *a) {
			//shvInfo() << "MENU ACTION:" << a;
			if(a == a_reloadNode) {
				ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ui->treeServers->currentIndex());
				if(nd)
					nd->reload();
			}
			else if(a == a_subscribeNode) {
				ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ui->treeServers->currentIndex());
				if(nd) {
					nd->serverNode()->addSubscription(nd->shvPath(), cp::Rpc::SIG_VAL_CHANGED, {});
				}
			}
			else if(a == a_callShvMethod) {
				ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ui->treeServers->currentIndex());
				if(nd) {
					shv::iotqt::rpc::ClientConnection *cc = nd->serverNode()->clientConnection();

					auto dlg = new DlgCallShvMethod(cc, this);
					dlg->setShvPath(nd->shvPath());
					dlg->open();
					connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
				}
			}
			else if(a == a_usersEditor) {
				ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ui->treeServers->currentIndex());
				if(nd) {
					shv::iotqt::rpc::ClientConnection *cc = nd->serverNode()->clientConnection();

					auto dlg = new DlgUsersEditor(this, cc);
					dlg->open();
					connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
				}
			}
			else if(a == a_rolesEditor) {
				ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ui->treeServers->currentIndex());
				if(nd) {
					shv::iotqt::rpc::ClientConnection *cc = nd->serverNode()->clientConnection();

					auto dlg = new DlgRolesEditor (this, cc);
					dlg->init(nd->shvPath() + "/etc/acl");
					dlg->open();
					connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
				}
			}
			else if(a == a_mountsEditor) {
				ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ui->treeServers->currentIndex());
				if(nd) {
					shv::iotqt::rpc::ClientConnection *cc = nd->serverNode()->clientConnection();

					auto dlg = new DlgMountsEditor(this, cc);
					dlg->init(nd->shvPath() + "/etc/acl");
					dlg->open();
					connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
				}
			}
			m->deleteLater();
		});
	}
}

void MainWindow::openNode(const QModelIndex &ix)
{
	ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ix);
	auto *bnd = qobject_cast<ShvBrokerNodeItem*>(nd);
	if(bnd) {
		AttributesModel *m = TheApp::instance()->attributesModel();
		if(bnd->openStatus() == ShvBrokerNodeItem::OpenStatus::Disconnected) {
			bnd->open();
		}
		else {
			bnd->close();
			m->load(nullptr);
		}
	}
}

void MainWindow::displayResult(const QModelIndex &ix)
{
	auto rv = qvariant_cast<cp::RpcValue>(ix.data(AttributesModel::RpcValueRole));
	displayValue(rv);
}

void MainWindow::displayValue(const shv::chainpack::RpcValue &rv)
{
	if(rv.isString() || rv.isBlob()) {
		auto *view = new TextEditDialog(this);
		view->setModal(false);
		view->setAttribute(Qt::WA_DeleteOnClose);
		view->setWindowIconText(tr("Result"));
		view->setReadOnly(true);
		auto [blob, size] = rv.asData();
		auto data = QByteArray::fromRawData(blob, size);
		if (rv.isString()) {
			view->setText(QString::fromUtf8(data));
		}
		else {
			view->setBlob(data);
		}
		view->show();
	}
	else {
		auto *view = new CponEditDialog(this);
		view->setModal(false);
		view->setAttribute(Qt::WA_DeleteOnClose);
		view->setWindowIconText(tr("Result"));
		view->setReadOnly(true);
		view->setValidateContent(false);
		QString cpon = QString::fromStdString(rv.toCpon("  "));
		view->setText(cpon);
		view->show();
	}
}

void MainWindow::editMethodParameters(const QModelIndex &ix)
{
	QVariant v = ix.data(AttributesModel::RpcValueRole);
	auto rv = qvariant_cast<cp::RpcValue>(v);

	QString path = TheApp::instance()->attributesModel()->path();
	QString method = TheApp::instance()->attributesModel()->method(ix.row());
	auto *dlg = new MethodParametersDialog(path, method, rv, this);
	dlg->setWindowTitle(tr("Parameters"));
	connect(dlg, &QDialog::finished, this, [this, dlg, ix](int result) {
		if (result == QDialog::Accepted) {
			cp::RpcValue val = dlg->value();
			if (val.isValid()) {
				std::string cpon = dlg->value().toCpon();
				ui->tblAttributes->model()->setData(ix, QString::fromStdString(cpon), Qt::EditRole);
			}
			else {
				ui->tblAttributes->model()->setData(ix, QString(), Qt::EditRole);
			}
		}
		dlg->deleteLater();
	});
	dlg->open();
}

void MainWindow::editStringParameter(const QModelIndex &ix)
{
	QVariant v = ix.data(AttributesModel::RpcValueRole);
	auto rv = qvariant_cast<cp::RpcValue>(v);
	QString cpon = QString::fromStdString(rv.asString());
	auto *dlg = new TextEditDialog(this);
	dlg->setWindowTitle(tr("Parameters"));
	dlg->setReadOnly(false);
	dlg->setText(cpon);
	connect(dlg, &QDialog::finished, this, [this, dlg, ix](int result) {
		if (result == QDialog::Accepted) {
			auto rv = cp::RpcValue(dlg->text().toStdString());
			auto cpon =  QString::fromStdString(rv.toCpon());
			ui->tblAttributes->model()->setData(ix, cpon, Qt::EditRole);
		}
		dlg->deleteLater();
	});
	dlg->show();
}

void MainWindow::editCponParameters(const QModelIndex &ix)
{
	QVariant v = ix.data(AttributesModel::RpcValueRole);
	auto rv = qvariant_cast<cp::RpcValue>(v);
	QString cpon = rv.isValid()? QString::fromStdString(rv.toCpon("  ")): QString();
	auto *dlg = new CponEditDialog(this);
	dlg->setWindowTitle(tr("Parameters"));
	dlg->setReadOnly(false);
	dlg->setValidateContent(true);
	dlg->setText(cpon);
	connect(dlg, &QDialog::finished, this, [this, dlg, ix](int result) {
		if (result == QDialog::Accepted) {
			auto cpon = dlg->text();
			ui->tblAttributes->model()->setData(ix, cpon, Qt::EditRole);
		}
		dlg->deleteLater();
	});
	dlg->show();
}

void MainWindow::onAttributesTableContextMenu(const QPoint &point)
{
	QModelIndex index = ui->tblAttributes->indexAt(point);
	if (index.isValid() && index.column() == AttributesModel::ColMethodName) {
		QMenu menu(this);
		menu.addAction(tr("Method description"));
		if (menu.exec(ui->tblAttributes->viewport()->mapToGlobal(point))) {
			QString s = index.data(Qt::ToolTipRole).toString();
			if(s.isEmpty())
				s = tr("Method description no available.");
			auto *view = new TextEditDialog(this);
			view->setModal(true);
			view->setAttribute(Qt::WA_DeleteOnClose);
			view->setWindowIconText(tr("Method description"));
			view->setReadOnly(true);
			view->setText(s);
			view->show();
		}
	}
	else if (index.isValid() && index.column() == AttributesModel::ColResult) {
		QMenu menu(this);
		auto *a_view_result = menu.addAction(tr("View result"));
		auto *a_save_result_binary = menu.addAction(tr("Save binary result"));
		auto *a_save_result_chainpack = menu.addAction(tr("Save result as ChainPack"));
		auto *a_save_result_cpon = menu.addAction(tr("Save result as Cpon"));
		auto *a = menu.exec(ui->tblAttributes->viewport()->mapToGlobal(point));
		if (a == a_view_result) {
			displayResult(index);
			return;
		}
		auto save_file = [this](const QString &ext, const std::string &data, const std::string &file_name = {}) {
			static QString recent_dir;
			const QString full_path = recent_dir + '/' + QString::fromStdString(file_name);
			QString fn = QFileDialog::getSaveFileName(this, tr("Save File"), full_path, ext);
			if(!fn.isEmpty()) {
				recent_dir = QFileInfo(fn).absolutePath();
				std::ofstream os(fn.toStdString(), std::ios::binary);
				if (os) {
					os.write(data.data(), data.size());
				}
			}
		};
		if (a == a_save_result_binary) {
			QVariant v = index.data(AttributesModel::RpcValueRole);
			const auto rpc_val = qvariant_cast<cp::RpcValue>(v);
			const std::string &s = rpc_val.toString();
			const std::string file_name = rpc_val.metaValue("fileName").asString();
			save_file(QString(), s, file_name);
			return;
		}
		if (a == a_save_result_chainpack) {
			QVariant v = index.data(AttributesModel::RpcValueRole);
			const auto rpc_val = qvariant_cast<cp::RpcValue>(v);
			const std::string s = rpc_val.toChainPack();
			const std::string file_name = rpc_val.metaValue("fileName").asString();
			save_file(tr("ChainPack files (*.chpk)"), s, file_name + ".chpk");
			return;
		}
		if (a == a_save_result_cpon) {
			QVariant v = index.data(AttributesModel::RpcValueRole);
			const auto rpc_val = qvariant_cast<cp::RpcValue>(v);
			const std::string s = rpc_val.toCpon();
			const std::string file_name = rpc_val.metaValue("fileName").asString();
			save_file(tr("Cpon files (*.cpon)"), s, file_name + ".cpon");
			return;
		}
	}
	else if (index.isValid() && index.column() == AttributesModel::ColParams) {
		QMenu menu(this);
		QAction *a_par_ed = menu.addAction(tr("Parameters editor"));
		QAction *a_str_ed = menu.addAction(tr("String parameter editor"));
		QAction *a_cpon_ed = menu.addAction(tr("Cpon parameters editor"));
		QAction *a = menu.exec(ui->tblAttributes->viewport()->mapToGlobal(point));
		if (a == a_par_ed) {
			editMethodParameters(index);
		}
		else if (a == a_str_ed) {
			editStringParameter(index);
		}
		else if (a == a_cpon_ed) {
			editCponParameters(index);
		}
	}
}

void MainWindow::onNotificationsDoubleClicked(const QModelIndex &ix)
{
	if (ix.column() == RpcNotificationsModel::Columns::ColParams) {
		auto data = TheApp::instance()->rpcNotificationsModel()->data(ix, Qt::DisplayRole);
		cp::RpcValue v = cp::RpcValue::fromCpon(data.toString().toStdString());
		displayValue(v);
	}
}

void MainWindow::onShvTreeViewCurrentSelectionChanged(const QModelIndex &curr_ix, const QModelIndex &prev_ix)
{
	Q_UNUSED(prev_ix)
	ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(curr_ix);
	if(nd) {
		AttributesModel *m = TheApp::instance()->attributesModel();
		ui->edAttributesShvPath->setText(QString::fromStdString(nd->shvPath()));
		auto *bnd = qobject_cast<ShvBrokerNodeItem*>(nd);
		if(bnd) {
			// hide attributes for server nodes
			//ui->edAttributesShvPath->setText(QString());
			m->load(bnd->isOpen()? bnd: nullptr);
		}
		else {
			//ui->edAttributesShvPath->setText(QString::fromStdString(nd->shvPath()));
			m->load(nd);
		}
	}
}

void MainWindow::editServer(ShvBrokerNodeItem *srv, bool copy_server)
{
	shvLogFuncFrame() << srv;
	QVariantMap broker_props;
	if(srv) {
		broker_props = srv->brokerProperties();
	}
	auto *dlg = new DlgBrokerProperties(this);
	dlg->setBrokerProperties(broker_props);
	connect(dlg, &QDialog::finished, this, [this, dlg, srv, copy_server](int result) {
		if(result == QDialog::Accepted) {
			QVariantMap broker_props = dlg->brokerProperties();
			if(!srv || copy_server)
				TheApp::instance()->serverTreeModel()->createConnection(broker_props);
			else
				srv->setBrokerProperties(broker_props);
			saveSettings();
		}
		dlg->deleteLater();
	});
	dlg->open();
}

void MainWindow::closeEvent(QCloseEvent *ev)
{
	saveSettings();
	Super::closeEvent(ev);
}

void MainWindow::saveSettings()
{
	QSettings settings;
	TheApp::instance()->saveSettings(settings);
	QByteArray ba = saveState();
	//shvInfo() << "saving wstate:" << ba.toHex().toStdString();
	settings.setValue(QStringLiteral("ui/mainWindow/state"), ba);
	settings.setValue(QStringLiteral("ui/mainWindow/geometry"), saveGeometry());
}

void MainWindow::openLogInspector()
{
	ShvNodeItem *nd = TheApp::instance()->serverTreeModel()->itemFromIndex(ui->treeServers->currentIndex());
	if(nd) {
		shv::iotqt::rpc::ClientConnection *cc = nd->serverNode()->clientConnection();
		auto dlg = new shv::visu::logview::DlgLogInspector(ui->edAttributesShvPath->text(), this);
		connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
		dlg->setRpcConnection(cc);
		dlg->open();
	}
}

class NodeChildLoader : public QObject
{
	Q_OBJECT
public:
	NodeChildLoader(ShvNodeItem *parent_node, const QStringList &dirs)
		: m_parentNode(parent_node)
		, m_dirs(dirs)
	{}
	Q_SIGNAL void finished(ShvNodeItem *last_node);
	void start()
	{
		shvDebug() << "start:" << m_dirs.join(',');
		if(m_dirs.isEmpty()) {
			deleteLater();
			return;
		}
		auto dir = m_dirs.first().toStdString();
		if(m_parentNode->childCount() > 0) {
			if(auto *nd = m_parentNode->childAt(dir); nd) {
				shvDebug() << "found child:" << dir;
				m_dirs.takeFirst();
				if(m_dirs.isEmpty()) {
					shvDebug() << "FINISH";
					emit finished(nd);
					deleteLater();
				}
				else {
					m_parentNode = nd;
					start();
				}
			}
			else {
				shvWarning() << "Invalid dir:" << dir;
				deleteLater();
			}
		}
		else {
			shvDebug() << "NOT found child:" << dir;
			connect(m_parentNode, &ShvNodeItem::childrenLoaded, this, [this]() {
				if(m_parentNode->childCount() == 0) {
					shvWarning() << "Missing dirs:" << m_dirs.join(',');
					deleteLater();
				}
				else {
					start();
				}
			});
			m_parentNode->loadChildren();
		}
	}
private:
	ShvNodeItem *m_parentNode;
	QStringList m_dirs;
};

void MainWindow::gotoShvPath()
{
	try {
		auto *view = ui->treeServers;
		auto *model = TheApp::instance()->serverTreeModel();
		auto ix = TheApp::instance()->serverTreeModel()->itemFromIndex(ui->treeServers->currentIndex());
		auto *server_node = ix->serverNode();
		auto path = ui->edAttributesShvPath->text().toStdString();
		QStringList dirs;
		for(const auto &d : shv::core::utils::ShvPath::split(path)) {
			dirs << QString::fromStdString(std::string(d));
		}
		auto *ldr = new NodeChildLoader(server_node, dirs);
		connect(ldr, &NodeChildLoader::finished, this, [model, view](ShvNodeItem *last_node) {
			if(auto last_ix = model->indexFromItem(last_node); last_ix.isValid()) {
				view->scrollTo(last_ix);
				view->setCurrentIndex(last_ix);
			}
		});
		ldr->start();
	}
	catch(const std::exception &e) {
		shvError() << __PRETTY_FUNCTION__ << "error:" << e.what();
	}
}

void MainWindow::onActHelpAbout_triggered()
{
	auto title = QCoreApplication::applicationName();
	auto text = "<p><b>" + QCoreApplication::applicationName() + "</b></p>"
		"<p>ver. " + QCoreApplication::applicationVersion() + "</p>"
#ifdef GIT_COMMIT
		"<p>git commit: " + SHV_EXPAND_AND_QUOTE(GIT_COMMIT) + "</p>"
#endif
		"<p>Silicon Heaven Swiss Knife</p>"
		"<p>2019 Elektroline a.s.</p>"
		"<p><a href=\"https://github.com/silicon-heaven\">github.com/silicon-heaven</a></p>";

#ifdef Q_OS_WASM
	// Can't use QMessageBox::about here, because of it uses exec().
	auto* msgBox = new QMessageBox(QMessageBox::Information, title, text, QMessageBox::NoButton, this);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    QIcon icon = msgBox->windowIcon();
    QSize size = icon.actualSize(QSize(64, 64));
    msgBox->setIconPixmap(icon.pixmap(size));
	msgBox->open();
#else
	QMessageBox::about(this, title, text);
#endif
}

#include "mainwindow.moc"
