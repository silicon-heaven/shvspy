#include "rolehierarchyheader.h"

#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStyle>
#include <QToolButton>

RoleHierarchyHeader::RoleHierarchyHeader(QWidget *parent)
	: Super(Qt::Horizontal, parent)
	, m_backButton(new QToolButton(viewport()))
	, m_closeButton(new QToolButton(viewport()))
{
	m_backButton->setFixedSize(20, 20);
	m_backButton->setIconSize(QSize(14, 14));
	m_backButton->setAutoRaise(true);
	m_backButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
	m_backButton->setToolTip(tr("Back to previous role"));

	QFont f = font();
	f.setBold(true);
	setFont(f);

	m_closeButton->setFixedSize(20, 20);
	m_closeButton->setIconSize(QSize(14, 14));
	m_closeButton->setAutoRaise(true);
	m_closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
	m_closeButton->setToolTip(tr("Close role hierarchy"));

	setSectionsClickable(false);
	setStretchLastSection(true);
	connect(this, &QHeaderView::geometriesChanged, this, [this]() { updateActionGeometry(); });
	updateActionGeometry();
}

QToolButton *RoleHierarchyHeader::backButton() const
{
	return m_backButton;
}

QToolButton *RoleHierarchyHeader::closeButton() const
{
	return m_closeButton;
}

void RoleHierarchyHeader::resizeEvent(QResizeEvent *event)
{
	QHeaderView::resizeEvent(event);
	updateActionGeometry();
}

void RoleHierarchyHeader::showEvent(QShowEvent *event)
{
	QHeaderView::showEvent(event);
	updateActionGeometry();
}

namespace {
constexpr int BUTTON_SIZE = 20;
constexpr int BUTTON_MARGIN = 3;
}

void RoleHierarchyHeader::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
	if (!rect.isValid()) {
			return;
	}

	QStyleOptionHeader opt;
	initStyleOption(&opt);
	opt.rect = rect;
	opt.section = logicalIndex;
	opt.text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
	opt.textAlignment = defaultAlignment();
	opt.position = QStyleOptionHeader::OnlyOneSection;

	painter->save();
	style()->drawControl(QStyle::CE_HeaderSection, &opt, painter, this);
	QRect label_rect = rect;
	label_rect.setLeft(rect.left() + BUTTON_SIZE + BUTTON_MARGIN * 2);
	label_rect.setRight(rect.right() - BUTTON_SIZE - BUTTON_MARGIN * 2);
	opt.rect = label_rect;
	style()->drawControl(QStyle::CE_HeaderLabel, &opt, painter, this);
	painter->restore();
}

void RoleHierarchyHeader::updateActionGeometry()
{
	const QRect viewport_rect = viewport()->rect();
	const int button_y = (viewport_rect.height() - BUTTON_SIZE) / 2;

	m_backButton->setGeometry(BUTTON_MARGIN, button_y, BUTTON_SIZE, BUTTON_SIZE);
	m_closeButton->setGeometry(viewport_rect.width() - BUTTON_MARGIN - BUTTON_SIZE, button_y, BUTTON_SIZE, BUTTON_SIZE);
}
