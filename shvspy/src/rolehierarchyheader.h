#pragma once

#include <QHeaderView>

class QLabel;
class QToolButton;

class RoleHierarchyHeader : public QHeaderView
{
	using Super = QHeaderView;

public:
	explicit RoleHierarchyHeader(QWidget *parent = nullptr);

	QToolButton *backButton() const;
	QToolButton *closeButton() const;

protected:
	void resizeEvent(QResizeEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;

private:
	void updateActionGeometry();

	QToolButton *m_backButton;
	QToolButton *m_closeButton;
};
