/*
    SPDX-FileCopyrightText: 2008 Marco Gittler <g.marco@freenet.de>
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef GRAPHICSSCENERECTMOVE_H
#define GRAPHICSSCENERECTMOVE_H

#include <QGraphicsEffect>
#include <QGraphicsScene>
#include <QGraphicsSvgItem>
#include <QGraphicsTextItem>

enum resizeModes { NoResize = 0, TopLeft, BottomLeft, TopRight, BottomRight, Left, Right, Up, Down };
enum TITLETOOL { TITLE_SELECT = 0, TITLE_RECTANGLE = 1, TITLE_TEXT = 2, TITLE_IMAGE = 3, TITLE_ELLIPSE = 4 };

class MyQGraphicsEffect : public QGraphicsEffect
{
public:
    explicit MyQGraphicsEffect(QObject *parent = nullptr);
    void setOffset(int xOffset, int yOffset, int blur);
    void setShadow(const QImage &image);

protected:
    void draw(QPainter *painter) override;

private:
    int m_xOffset{0};
    int m_yOffset{0};
    int m_blur{0};
    QImage m_shadow;
};

class MyTextItem : public QGraphicsTextItem
{
    Q_OBJECT
public:
    MyTextItem(const QString &, QGraphicsItem *parent = nullptr);
    void setAlignment(Qt::Alignment alignment);
    /** @brief returns an extended bounding containing shadow */
    QRectF boundingRect() const override;
    /** @brief returns the normal bounding rect around text */
    QRectF baseBoundingRect() const;
    Qt::Alignment alignment() const;
    void updateShadow(bool enabled, int blur, int xoffset, int yoffset, QColor color);
    QStringList shadowInfo() const;
    void loadShadow(const QStringList &info);
    void updateTW(bool enabled, int step, int mode, int sigma, int seed);
    QStringList twInfo() const;
    void loadTW(const QStringList &info);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *evt) override;
    void setTextColor(const QColor &col);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *w) override;

private:
    Qt::Alignment m_alignment;
    QPoint m_shadowOffset;
    int m_shadowBlur;
    QColor m_shadowColor;
    QPainterPath m_path;
    MyQGraphicsEffect *m_shadowEffect;
    void updateShadow();
    void blurShadow(QImage &image, int radius);
    void refreshFormat();
    bool m_tw_enabled;
    int m_tw_step;
    int m_tw_mode;
    int m_tw_sigma;
    int m_tw_seed;

public slots:
    void doUpdateGeometry();
    void updateGeometry();
};

class MyRectItem : public QGraphicsRectItem
{
public:
    explicit MyRectItem(QGraphicsItem *parent = nullptr);
    void setRect(const QRectF &rectangle);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QRectF m_rect;
};

class MyEllipseItem : public QGraphicsEllipseItem
{
public:
    explicit MyEllipseItem(QGraphicsItem *parent = nullptr);
    void setRect(const QRectF &rectangle);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QRectF m_ellipse;
};

class MyPixmapItem : public QGraphicsPixmapItem
{
public:
    MyPixmapItem(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
};

class MySvgItem : public QGraphicsSvgItem
{
public:
    MySvgItem(const QString &fileName = QString(), QGraphicsItem *parent = nullptr);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
};

class GraphicsSceneRectMove : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit GraphicsSceneRectMove(QObject *parent = nullptr);
    void setSelectedItem(QGraphicsItem *item);
    void setScale(double s);
    void setZoom(double s);
    void setTool(TITLETOOL tool);
    TITLETOOL tool() const;
    /** @brief Get out of text edit mode. If reset is true, we also unselect all items */
    void clearTextSelection(bool reset = true);
    int gridSize() const;
    void addNewItem(QGraphicsItem *item);

public slots:
    void slotUpdateFontSize(int s);
    void slotUseGrid(bool enableGrid);

protected:
    void keyPressEvent(QKeyEvent *keyEvent) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) override;
    /** @brief Resizes and moves items */
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void wheelEvent(QGraphicsSceneWheelEvent *wheelEvent) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *contextMenuEvent) override;

private:
    void setCursor(const QCursor &);
    double m_zoom;
    QGraphicsItem *m_selectedItem{nullptr};
    resizeModes m_resizeMode{NoResize};
    resizeModes m_possibleAction{NoResize};
    QPointF m_sceneClickPoint;
    TITLETOOL m_tool{TITLE_RECTANGLE};
    QPointF m_clickPoint;
    int m_fontSize;
    int m_gridSize{20};
    bool m_createdText{false};
    bool m_moveStarted{false};
    bool m_pan{false};

signals:
    void itemMoved();
    void sceneZoom(bool);
    void newRect(QGraphicsRectItem *);
    void newEllipse(QGraphicsEllipseItem *);
    void newText(MyTextItem *);
    void actionFinished();
    void doubleClickEvent();
};

#endif
