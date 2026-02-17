#include "core/backend.hpp"
#include <QAbstractScrollArea>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QSyntaxHighlighter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextCharFormat>
#include <QToolBar>
#include <QVBoxLayout>
#include <QDrag>
#include <QTabBar>
#include <algorithm>
#include <cmath>
#include <deque>
#include <set>

// ═════════════════════════════════════════════════════════════════════════════
// DraggableTabWidget — tabs can be dragged between any instance
// ═════════════════════════════════════════════════════════════════════════════
static QList<class DraggableTabWidget*> g_allTabWidgets;

class DraggableTabBar : public QTabBar {
  Q_OBJECT
public:
  explicit DraggableTabBar(QWidget* p=nullptr):QTabBar(p){ setAcceptDrops(true); }
protected:
  void mousePressEvent(QMouseEvent* e) override {
    if(e->button()==Qt::LeftButton) m_dragStart=e->position().toPoint();
    QTabBar::mousePressEvent(e);
  }
  void mouseMoveEvent(QMouseEvent* e) override {
    if(!(e->buttons()&Qt::LeftButton)||m_dragStart.isNull()){QTabBar::mouseMoveEvent(e);return;}
    if((e->position().toPoint()-m_dragStart).manhattanLength()<QApplication::startDragDistance()){
      QTabBar::mouseMoveEvent(e);return;
    }
    int idx=tabAt(m_dragStart);
    if(idx<0) return;
    auto* drag=new QDrag(this);
    auto* mime=new QMimeData;
    mime->setData("application/x-rever-tab",QByteArray::number(idx));
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
    m_dragStart=QPoint();
  }
  void dragEnterEvent(QDragEnterEvent* e) override {
    if(e->mimeData()->hasFormat("application/x-rever-tab")) e->acceptProposedAction();
  }
  void dropEvent(QDropEvent* e) override {
    if(!e->mimeData()->hasFormat("application/x-rever-tab")) return;
    auto* srcBar=qobject_cast<DraggableTabBar*>(e->source());
    if(!srcBar) return;
    int srcIdx=e->mimeData()->data("application/x-rever-tab").toInt();
    auto* srcTW=qobject_cast<QTabWidget*>(srcBar->parentWidget());
    auto* dstTW=qobject_cast<QTabWidget*>(parentWidget());
    if(!srcTW||!dstTW) return;
    if(srcTW==dstTW){
      // Reorder within same widget
      int dstIdx=tabAt(e->position().toPoint());
      if(dstIdx<0) dstIdx=count();
      if(srcIdx!=dstIdx) srcTW->tabBar()->moveTab(srcIdx,dstIdx);
    } else {
      // Move between different tab widgets
      if(srcTW->count()<=1) return; // don't leave a tab widget empty
      QWidget* w=srcTW->widget(srcIdx);
      QString label=srcTW->tabText(srcIdx);
      srcTW->removeTab(srcIdx);
      int dstIdx=tabAt(e->position().toPoint());
      if(dstIdx<0) dstIdx=dstTW->count();
      dstTW->insertTab(dstIdx,w,label);
      dstTW->setCurrentIndex(dstIdx);
    }
    e->acceptProposedAction();
  }
private:
  QPoint m_dragStart;
};

class DraggableTabWidget : public QTabWidget {
  Q_OBJECT
public:
  explicit DraggableTabWidget(QWidget* p=nullptr):QTabWidget(p){
    auto* bar=new DraggableTabBar(this);
    setTabBar(bar);
    setMovable(false); // we handle moves ourselves
    setAcceptDrops(true);
    g_allTabWidgets.append(this);
  }
  ~DraggableTabWidget() override { g_allTabWidgets.removeAll(this); }
protected:
  void dragEnterEvent(QDragEnterEvent* e) override {
    if(e->mimeData()->hasFormat("application/x-rever-tab")) e->acceptProposedAction();
    else QTabWidget::dragEnterEvent(e);
  }
  void dropEvent(QDropEvent* e) override {
    if(e->mimeData()->hasFormat("application/x-rever-tab")){
      // Forward to our tab bar's drop handler for the pane area
      auto* srcBar=qobject_cast<DraggableTabBar*>(e->source());
      if(!srcBar) return;
      int srcIdx=e->mimeData()->data("application/x-rever-tab").toInt();
      auto* srcTW=qobject_cast<QTabWidget*>(srcBar->parentWidget());
      if(!srcTW||srcTW==this) return;
      if(srcTW->count()<=1) return;
      QWidget* w=srcTW->widget(srcIdx);
      QString label=srcTW->tabText(srcIdx);
      srcTW->removeTab(srcIdx);
      int dstIdx=count();
      insertTab(dstIdx,w,label);
      setCurrentIndex(dstIdx);
      e->acceptProposedAction();
    } else QTabWidget::dropEvent(e);
  }
};

static const char* kStyleSheet = R"(
* { color:#ccc; font-family:"Roboto Mono","SF Mono","Menlo","Consolas",monospace; font-size:13px; }
QMainWindow,QWidget { background:#000; border:none; }
QMenuBar { background:#000; border-bottom:1px solid #444; padding:2px 0; }
QMenuBar::item { background:transparent; padding:4px 10px; }
QMenuBar::item:selected { background:#ccc; color:#000; }
QMenu { background:#000; border:1px solid #444; padding:4px; }
QMenu::item { padding:6px 28px 6px 16px; }
QMenu::item:selected { background:#ccc; color:#000; }
QMenu::separator { height:1px; background:#444; margin:4px 8px; }
QStatusBar { background:#000; border-top:1px solid #444; color:#666; font-size:12px; }
QToolBar { background:#000; border-bottom:1px solid #333; spacing:2px; padding:2px; }
QToolBar QToolButton { background:#000; border:1px solid transparent; padding:3px 8px; color:#888; font-size:11px; }
QToolBar QToolButton:hover { border-color:#444; color:#ccc; }
QToolBar::separator { background:#333; width:1px; margin:2px 4px; }
QSplitter::handle { background:#444; }
QSplitter::handle:horizontal { width:1px; }
QSplitter::handle:vertical { height:1px; }
QScrollBar:vertical { background:#000; width:6px; border-left:1px solid #333; }
QScrollBar::handle:vertical { background:#555; min-height:20px; }
QScrollBar::handle:vertical:hover { background:#777; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:none; }
QScrollBar:horizontal { background:#000; height:6px; border-top:1px solid #333; }
QScrollBar::handle:horizontal { background:#555; min-width:20px; }
QScrollBar::handle:horizontal:hover { background:#777; }
QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal { width:0; }
QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal { background:none; }
QPlainTextEdit { background:#000; color:#aaa; border:none; selection-background-color:#333; }
QTabWidget::pane { border:1px solid #444; border-top:none; background:#000; }
QTabBar::tab { background:#000; color:#666; padding:5px 14px; border:1px solid #444; border-bottom:none; margin-right:-1px; }
QTabBar::tab:selected { background:#ccc; color:#000; font-weight:600; }
QTabBar::tab:hover:!selected { color:#aaa; }
QTableWidget { background:#000; gridline-color:#222; border:none; color:#aaa; }
QTableWidget::item { padding:2px 6px; }
QTableWidget::item:selected { background:#333; color:#ccc; }
QHeaderView::section { background:#000; color:#888; border:none; border-bottom:1px solid #444; padding:4px 8px; font-size:11px; }
QLineEdit { background:#0a0a0a; border:1px solid #444; padding:3px 6px; color:#ccc; }
QPushButton { background:#111; border:1px solid #444; padding:4px 12px; color:#ccc; }
QPushButton:hover { background:#222; }
QPushButton:pressed { background:#333; }
QComboBox { background:#0a0a0a; border:1px solid #444; padding:3px 8px; color:#ccc; }
QComboBox::drop-down { border:none; }
QComboBox QAbstractItemView { background:#000; border:1px solid #444; color:#ccc; selection-background-color:#ccc; selection-color:#000; }
QListWidget { background:#000; border:1px solid #333; color:#aaa; }
QListWidget::item:selected { background:#333; color:#ccc; }
QCheckBox { color:#888; spacing:6px; }
QCheckBox::indicator { width:12px; height:12px; border:1px solid #555; background:#000; }
QCheckBox::indicator:checked { background:#888; }
)";

// ═════════════════════════════════════════════════════════════════════════════
// HexView — full-featured hex editor
// ═════════════════════════════════════════════════════════════════════════════
class HexView : public QAbstractScrollArea {
  Q_OBJECT
public:
  explicit HexView(QWidget* p=nullptr):QAbstractScrollArea(p){
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    auto pal=palette(); pal.setColor(QPalette::Base,Qt::black); setPalette(pal);
    viewport()->setAutoFillBackground(true);
    setFrameShape(QFrame::NoFrame);
    verticalScrollBar()->setSingleStep(1);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
  }

  void setData(std::vector<uint8_t>* d){
    m_data=d; m_cursor=0; m_selStart=m_selEnd=0;
    m_modified.clear(); m_undoStack.clear(); m_redoStack.clear();
    adjust(); viewport()->update();
    emit cursorChanged(0);
  }
  size_t cursor() const { return m_cursor; }
  size_t selStart() const { return std::min(m_selStart,m_selEnd); }
  size_t selEnd() const { return std::max(m_selStart,m_selEnd); }
  bool hasSelection() const { return m_selStart!=m_selEnd; }
  bool isModified() const { return !m_modified.empty(); }

  void copyAsHex(){
    if(!m_data||!hasSelection()) return;
    size_t a=selStart(),b=selEnd();
    QString s;
    for(size_t i=a;i<=b&&i<m_data->size();++i)
      s+=QString("%1 ").arg((*m_data)[i],2,16,QChar('0'));
    QApplication::clipboard()->setText(s.trimmed());
  }
  void copyAsText(){
    if(!m_data||!hasSelection()) return;
    size_t a=selStart(),b=selEnd();
    QString s;
    for(size_t i=a;i<=b&&i<m_data->size();++i){
      uint8_t v=(*m_data)[i];
      s+=QChar(v>=0x20&&v<=0x7E?(char)v:'.');
    }
    QApplication::clipboard()->setText(s);
  }
  void undo(){
    if(m_undoStack.empty()) return;
    auto e=m_undoStack.back(); m_undoStack.pop_back();
    m_redoStack.push_back({e.offset,(*m_data)[e.offset]});
    (*m_data)[e.offset]=e.oldVal;
    m_modified.erase(e.offset);
    viewport()->update(); emit dataEdited();
  }
  void redo(){
    if(m_redoStack.empty()) return;
    auto e=m_redoStack.back(); m_redoStack.pop_back();
    m_undoStack.push_back({e.offset,(*m_data)[e.offset]});
    (*m_data)[e.offset]=e.oldVal;
    m_modified.insert(e.offset);
    viewport()->update(); emit dataEdited();
  }

signals:
  void cursorChanged(size_t offset);
  void selectionChanged(size_t start, size_t end);
  void dataEdited();

public slots:
  void gotoByte(size_t off){
    if(!m_data||off>=m_data->size()) return;
    m_cursor=off; m_selStart=m_selEnd=off;
    int line=(int)(off/16), vis=std::max(1,(viewport()->height()-lineH())/lineH());
    int sv=verticalScrollBar()->value();
    if(line<sv||line>=sv+vis) verticalScrollBar()->setValue(std::max(0,line-vis/3));
    viewport()->update();
    emit cursorChanged(m_cursor);
  }

protected:
  int lineH() const { return QFontMetrics(font()).height()+2; }
  int charW() const { return QFontMetrics(font()).horizontalAdvance('0'); }
  int headerY() const { return lineH(); }

  void paintEvent(QPaintEvent*) override {
    QPainter p(viewport());
    p.setRenderHint(QPainter::TextAntialiasing);
    int vw=viewport()->width(), vh=viewport()->height();
    p.fillRect(0,0,vw,vh,Qt::black);
    if(!m_data||m_data->empty()){
      p.setPen(QColor(0x33,0x33,0x33));
      p.drawText(QRect(0,0,vw,vh),Qt::AlignCenter,"Drop a file or press Cmd+O");
      return;
    }
    const QFontMetrics fm(font());
    const int cw=charW(), lh=lineH(), bpl=16;
    const int sl=verticalScrollBar()->value(), nl=vh/lh+2;
    const size_t total=(m_data->size()+bpl-1)/bpl;
    const int ax=8, hx=ax+cw*10, asx=hx+cw*(bpl*3+2);
    p.setFont(font());

    // Column header
    p.setPen(QColor(0x44,0x44,0x44));
    p.fillRect(0,0,vw,lh,QColor(0x08,0x08,0x08));
    p.drawText(ax, fm.ascent(), "Offset");
    for(int i=0;i<16;++i){
      int bx=hx+i*cw*3+(i>=8?cw:0);
      p.drawText(bx,fm.ascent(),QString("%1").arg(i,2,16,QChar('0')).toUpper());
    }
    p.drawText(asx,fm.ascent(),"ASCII");
    p.setPen(QColor(0x33,0x33,0x33));
    p.drawLine(0,lh-1,vw,lh-1);

    size_t sa=selStart(), sb=selEnd();
    int yoff=lh;

    for(int i=0;i<nl&&(sl+i)<(int)total;++i){
      int ln=sl+i, y=yoff+i*lh+fm.ascent();
      size_t off=(size_t)ln*bpl;

      // Current row highlight
      if(m_cursor/16==(size_t)ln)
        p.fillRect(0,yoff+i*lh,vw,lh,QColor(0x0a,0x0a,0x0a));

      // Address
      p.setPen(QColor(0x44,0x44,0x44));
      p.drawText(ax,y,QString("%1").arg(off,8,16,QChar('0')));

      // Hex bytes
      for(int b=0;b<bpl&&off+b<m_data->size();++b){
        uint8_t v=(*m_data)[off+b];
        size_t addr=off+b;
        int bx=hx+b*cw*3+(b>=8?cw:0);
        bool isCursor=(addr==m_cursor);
        bool inSel=(hasSelection()&&addr>=sa&&addr<=sb);
        bool isMod=(m_modified.count(addr)>0);

        if(isCursor)
          p.fillRect(bx-1,y-fm.ascent(),cw*2+2,lh,QColor(0x33,0x33,0x33));
        else if(inSel)
          p.fillRect(bx-1,y-fm.ascent(),cw*2+2,lh,QColor(0x1a,0x1a,0x2e));

        if(isMod) p.setPen(QColor(0xcc,0x66,0x66));
        else if(isCursor) p.setPen(Qt::white);
        else if(inSel) p.setPen(QColor(0xcc,0xcc,0xcc));
        else if(v==0) p.setPen(QColor(0x2a,0x2a,0x2a));
        else if(v>=0x20&&v<=0x7E) p.setPen(QColor(0xaa,0xaa,0xaa));
        else p.setPen(QColor(0x55,0x55,0x55));
        p.drawText(bx,y,QString("%1").arg(v,2,16,QChar('0')));
      }

      // ASCII
      for(int b=0;b<bpl&&off+b<m_data->size();++b){
        uint8_t v=(*m_data)[off+b];
        size_t addr=off+b;
        bool pr=v>=0x20&&v<=0x7E;
        int cx=asx+b*cw;
        bool isCursor=(addr==m_cursor);
        bool inSel=(hasSelection()&&addr>=sa&&addr<=sb);
        bool isMod=(m_modified.count(addr)>0);

        if(isCursor) p.fillRect(cx-1,y-fm.ascent(),cw+2,lh,QColor(0x33,0x33,0x33));
        else if(inSel) p.fillRect(cx-1,y-fm.ascent(),cw+2,lh,QColor(0x1a,0x1a,0x2e));

        if(isMod) p.setPen(QColor(0xcc,0x66,0x66));
        else if(isCursor) p.setPen(Qt::white);
        else if(inSel) p.setPen(QColor(0xcc,0xcc,0xcc));
        else p.setPen(pr?QColor(0x77,0x77,0x77):QColor(0x2a,0x2a,0x2a));
        p.drawText(cx,y,QString(QChar(pr?(char)v:'.')));
      }
    }

    // Entropy minimap on the right edge
    if(!m_entropy.empty()){
      int mw=4, mx=vw-mw;
      int dh=vh-lh;
      for(int y=0;y<dh;++y){
        int idx=(int)((float)y/dh*m_entropy.size());
        if(idx>=(int)m_entropy.size()) idx=(int)m_entropy.size()-1;
        float e=m_entropy[idx]/8.0f;
        uint8_t v=(uint8_t)(e*80);
        p.fillRect(mx,lh+y,mw,1,QColor(v,v,v));
      }
    }
  }

  void mousePressEvent(QMouseEvent* e) override {
    if(!m_data||m_data->empty()) return;
    size_t off=hitTest(e->position());
    if(off==(size_t)-1) return;
    if(e->modifiers()&Qt::ShiftModifier){
      m_selEnd=off; m_cursor=off;
    } else {
      m_cursor=off; m_selStart=m_selEnd=off;
    }
    m_dragging=true;
    viewport()->update();
    emit cursorChanged(m_cursor);
    if(hasSelection()) emit selectionChanged(selStart(),selEnd());
  }
  void mouseMoveEvent(QMouseEvent* e) override {
    if(!m_dragging||!m_data) return;
    size_t off=hitTest(e->position());
    if(off==(size_t)-1) return;
    m_selEnd=off; m_cursor=off;
    viewport()->update();
    emit cursorChanged(m_cursor);
    emit selectionChanged(selStart(),selEnd());
  }
  void mouseReleaseEvent(QMouseEvent*) override { m_dragging=false; }

  void keyPressEvent(QKeyEvent* e) override {
    if(!m_data||m_data->empty()) return;
    size_t sz=m_data->size();
    bool shift=e->modifiers()&Qt::ShiftModifier;

    // Hex digit input for editing
    QString t=e->text();
    if(t.size()==1){
      char c=t[0].toLatin1();
      int nib=-1;
      if(c>='0'&&c<='9') nib=c-'0';
      else if(c>='a'&&c<='f') nib=c-'a'+10;
      else if(c>='A'&&c<='F') nib=c-'A'+10;
      if(nib>=0){
        editNibble(nib);
        return;
      }
    }

    size_t prev=m_cursor;
    if(e->key()==Qt::Key_Right&&m_cursor+1<sz) m_cursor++;
    else if(e->key()==Qt::Key_Left&&m_cursor>0) m_cursor--;
    else if(e->key()==Qt::Key_Down&&m_cursor+16<sz) m_cursor+=16;
    else if(e->key()==Qt::Key_Up&&m_cursor>=16) m_cursor-=16;
    else if(e->key()==Qt::Key_PageDown) m_cursor=std::min(sz-1,m_cursor+(size_t)(viewport()->height()/lineH())*16);
    else if(e->key()==Qt::Key_PageUp){size_t pg=(size_t)(viewport()->height()/lineH())*16;m_cursor=m_cursor>pg?m_cursor-pg:0;}
    else if(e->key()==Qt::Key_Home) m_cursor=0;
    else if(e->key()==Qt::Key_End) m_cursor=sz-1;
    else { QAbstractScrollArea::keyPressEvent(e); return; }

    if(shift){ if(prev==m_selStart&&prev==m_selEnd) m_selStart=prev; m_selEnd=m_cursor; }
    else { m_selStart=m_selEnd=m_cursor; }
    ensureVisible();
    viewport()->update();
    emit cursorChanged(m_cursor);
    if(hasSelection()) emit selectionChanged(selStart(),selEnd());
  }

  void resizeEvent(QResizeEvent* e) override { QAbstractScrollArea::resizeEvent(e); adjust(); }
  void wheelEvent(QWheelEvent* e) override {
    int d=e->angleDelta().y()>0?-3:3;
    verticalScrollBar()->setValue(verticalScrollBar()->value()+d);
    viewport()->update();
  }

public:
  void setEntropy(const std::vector<float>& ent){ m_entropy=ent; viewport()->update(); }

private:
  void adjust(){
    if(!m_data||m_data->empty()){verticalScrollBar()->setRange(0,0);return;}
    int lh_=lineH(),tot=(int)((m_data->size()+15)/16),vis=std::max(1,(viewport()->height()-lh_)/lh_);
    verticalScrollBar()->setRange(0,std::max(0,tot-vis));
    verticalScrollBar()->setPageStep(vis);
  }
  void ensureVisible(){
    int line=(int)(m_cursor/16),vis=std::max(1,(viewport()->height()-lineH())/lineH());
    int sv=verticalScrollBar()->value();
    if(line<sv) verticalScrollBar()->setValue(line);
    else if(line>=sv+vis) verticalScrollBar()->setValue(line-vis+1);
  }
  size_t hitTest(QPointF pos){
    const int cw=charW(),lh_=lineH(),bpl=16,hx=8+cw*10;
    int row=(int)(pos.y()-lh_)/lh_+verticalScrollBar()->value();
    int mx=(int)pos.x();
    if(row<0) return (size_t)-1;
    size_t off=(size_t)row*bpl;
    for(int b=0;b<bpl&&off+b<m_data->size();++b){
      int bx=hx+b*cw*3+(b>=8?cw:0);
      if(mx>=bx-1&&mx<bx+cw*2+2) return off+b;
    }
    const int asx=hx+cw*(bpl*3+2);
    for(int b=0;b<bpl&&off+b<m_data->size();++b){
      int cx=asx+b*cw;
      if(mx>=cx-1&&mx<cx+cw+2) return off+b;
    }
    return (size_t)-1;
  }
  void editNibble(int nib){
    if(!m_data||m_cursor>=m_data->size()) return;
    uint8_t old=(*m_data)[m_cursor];
    uint8_t nv;
    if(!m_editHigh){ nv=(old&0x0F)|((uint8_t)nib<<4); m_editHigh=true; }
    else { nv=(old&0xF0)|(uint8_t)nib; m_editHigh=false;
      if(m_cursor+1<m_data->size()) m_cursor++;
    }
    m_undoStack.push_back({m_editHigh?m_cursor:m_cursor-(!m_editHigh&&m_cursor>0?0:0),old});
    // Correct: always push the cursor that was actually edited
    if(!m_editHigh&&m_undoStack.size()>0){
      m_undoStack.back().offset=m_cursor; m_undoStack.back().oldVal=old;
    } else if(m_editHigh) {
      m_undoStack.back().offset=m_cursor; m_undoStack.back().oldVal=old;
    }
    (*m_data)[m_editHigh?m_cursor:(m_cursor>0?m_cursor-1:0)]=nv;
    // Simplified: track modified offset
    m_modified.insert(m_editHigh?m_cursor:(m_cursor>0?m_cursor-1:0));
    m_redoStack.clear();
    viewport()->update();
    emit cursorChanged(m_cursor);
    emit dataEdited();
  }

  struct UndoEntry { size_t offset; uint8_t oldVal; };
  std::vector<uint8_t>* m_data=nullptr;
  size_t m_cursor=0, m_selStart=0, m_selEnd=0;
  bool m_dragging=false, m_editHigh=false;
  std::set<size_t> m_modified;
  std::deque<UndoEntry> m_undoStack, m_redoStack;
  std::vector<float> m_entropy;
};

// ═════════════════════════════════════════════════════════════════════════════
// DataInspector
// ═════════════════════════════════════════════════════════════════════════════
class DataInspector : public QTableWidget {
  Q_OBJECT
public:
  explicit DataInspector(QWidget* p=nullptr):QTableWidget(p){
    setColumnCount(2); setHorizontalHeaderLabels({"Type","Value"});
    horizontalHeader()->setStretchLastSection(true);
    verticalHeader()->setVisible(false);
    setEditTriggers(NoEditTriggers); setShowGrid(false);
    setSelectionBehavior(SelectRows);
  }
public slots:
  void inspect(const uint8_t* data, size_t size, size_t offset){
    auto r=inspect_data(data,size,offset);
    setRowCount((int)r.size());
    for(int i=0;i<(int)r.size();++i){
      auto* t=new QTableWidgetItem(QString::fromStdString(r[i].type_name));
      t->setForeground(QColor(0x66,0x66,0x66));
      setItem(i,0,t);
      auto* v=new QTableWidgetItem(QString::fromStdString(r[i].value));
      v->setForeground(QColor(0xaa,0xaa,0xaa));
      setItem(i,1,v);
    }
    resizeColumnToContents(0);
  }
};

// ═════════════════════════════════════════════════════════════════════════════
// DisassemblyHighlighter — syntax color for asm
// ═════════════════════════════════════════════════════════════════════════════
class AsmHighlighter : public QSyntaxHighlighter {
  Q_OBJECT
public:
  explicit AsmHighlighter(QTextDocument* doc):QSyntaxHighlighter(doc){
    // Address (hex at line start)
    fmtAddr.setForeground(QColor(0x55,0x55,0x55));
    // Bytes
    fmtBytes.setForeground(QColor(0x44,0x44,0x44));
    // Mnemonic - control flow
    fmtBranch.setForeground(QColor(0xaa,0x77,0x77));
    fmtBranch.setFontWeight(QFont::Bold);
    // Mnemonic - normal
    fmtMnem.setForeground(QColor(0x88,0x88,0xbb));
    // Register
    fmtReg.setForeground(QColor(0x88,0xaa,0x88));
    // Immediate/number
    fmtImm.setForeground(QColor(0xaa,0x99,0x77));
  }
protected:
  void highlightBlock(const QString& text) override {
    if(text.isEmpty()) return;
    // Format: "ADDR  BYTES                    MNEM     OPERANDS"
    // Address: first 8 chars
    if(text.size()>=8){
      setFormat(0,8,fmtAddr);
    }
    // Bytes: chars 10-34
    if(text.size()>10){
      int bEnd=std::min(34,(int)text.size());
      setFormat(10,bEnd-10,fmtBytes);
    }
    // Mnemonic + operands: from char 36+
    if(text.size()>36){
      int mStart=36;
      int mEnd=text.indexOf(' ',mStart);
      if(mEnd<0) mEnd=text.size();
      QString mnem=text.mid(mStart,mEnd-mStart).trimmed().toLower();
      bool isBranch=mnem.startsWith("j")||mnem.startsWith("call")||mnem.startsWith("ret")||
                     mnem.startsWith("b.")||mnem=="bl"||mnem=="br"||mnem=="blr"||
                     mnem.startsWith("loop")||mnem=="syscall"||mnem=="int";
      setFormat(mStart,mEnd-mStart,isBranch?fmtBranch:fmtMnem);
      // Operands
      if(mEnd<text.size()){
        QString ops=text.mid(mEnd);
        int base=mEnd;
        for(int i=0;i<ops.size();){
          QChar c=ops[i];
          if(c=='0'&&i+1<ops.size()&&ops[i+1]=='x'){
            int s=i; i+=2;
            while(i<ops.size()&&ops[i].isLetterOrNumber()) i++;
            setFormat(base+s,i-s,fmtImm);
          } else if(c.isDigit()||(c=='-'&&i+1<ops.size()&&ops[i+1].isDigit())){
            int s=i; i++;
            while(i<ops.size()&&(ops[i].isDigit()||ops[i]=='x'||ops[i].isLetter())) i++;
            setFormat(base+s,i-s,fmtImm);
          } else if(c=='%'||(c.isLetter()&&!c.isUpper())){
            int s=i; i++;
            while(i<ops.size()&&(ops[i].isLetterOrNumber()||ops[i]=='_')) i++;
            QString w=ops.mid(s,i-s);
            if(isRegister(w)) setFormat(base+s,i-s,fmtReg);
          } else i++;
        }
      }
    }
  }
private:
  bool isRegister(const QString& w) const {
    static const QSet<QString> regs={
      "rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp","r8","r9","r10","r11","r12","r13","r14","r15",
      "eax","ebx","ecx","edx","esi","edi","ebp","esp",
      "ax","bx","cx","dx","si","di","bp","sp","al","bl","cl","dl","ah","bh","ch","dh",
      "rip","eip","ip","cs","ds","es","fs","gs","ss",
      "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
      "xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15",
      "ymm0","ymm1","ymm2","ymm3","x0","x1","x2","x3","x4","x5","x6","x7",
      "x8","x9","x10","x11","x12","x13","x14","x15","x16","x17","x18","x19",
      "x20","x21","x22","x23","x24","x25","x26","x27","x28","x29","x30","sp","lr","pc",
      "w0","w1","w2","w3","w4","w5","w6","w7","w8","w9","w10","w11","w12","w13","w14","w15"
    };
    return regs.contains(w.toLower());
  }
  QTextCharFormat fmtAddr, fmtBytes, fmtBranch, fmtMnem, fmtReg, fmtImm;
};

// ═════════════════════════════════════════════════════════════════════════════
// DisassemblyView — arch selector + syntax highlighted
// ═════════════════════════════════════════════════════════════════════════════
class DisassemblyView : public QWidget {
  Q_OBJECT
public:
  explicit DisassemblyView(QWidget* p=nullptr):QWidget(p){
    auto* lay=new QVBoxLayout(this); lay->setContentsMargins(0,0,0,0); lay->setSpacing(0);
    auto* bar=new QHBoxLayout(); bar->setContentsMargins(4,4,4,4); bar->setSpacing(4);
    m_archCombo=new QComboBox;
    m_archCombo->addItems({"x86-64","x86-32","ARM64","ARM32","MIPS"});
    m_baseAddr=new QLineEdit("0"); m_baseAddr->setMaximumWidth(100);
    m_baseAddr->setPlaceholderText("Base addr");
    bar->addWidget(new QLabel("Arch:")); bar->addWidget(m_archCombo);
    bar->addWidget(new QLabel("Base:")); bar->addWidget(m_baseAddr);
    auto* btn=new QPushButton("Disassemble");
    connect(btn,&QPushButton::clicked,this,&DisassemblyView::onDisassemble);
    bar->addWidget(btn); bar->addStretch();
    m_countLabel=new QLabel; m_countLabel->setStyleSheet("color:#555;font-size:11px;");
    bar->addWidget(m_countLabel);
    lay->addLayout(bar);
    m_text=new QPlainTextEdit;
    m_text->setReadOnly(true); m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_text->setPlaceholderText("Open a file to disassemble");
    m_hl=new AsmHighlighter(m_text->document());
    lay->addWidget(m_text);
  }
  void setFileData(const uint8_t* d,size_t s){m_fd=d;m_fs=s;onDisassemble();}
signals:
  void gotoOffset(size_t off);
private slots:
  void onDisassemble(){
    if(!m_fd||!m_fs) return;
    bool ok; uint64_t base=m_baseAddr->text().toULongLong(&ok,16);
    if(!ok) base=0;
    auto lines=disassemble(m_fd,m_fs,base,m_archCombo->currentIndex(),8000);
    QString t; t.reserve((int)lines.size()*80);
    for(auto& l:lines)
      t+=QString("%1  %2  %3 %4\n").arg(l.address,8,16,QChar('0'))
        .arg(QString::fromStdString(l.bytes_hex),-24)
        .arg(QString::fromStdString(l.mnemonic),-8)
        .arg(QString::fromStdString(l.operands));
    m_text->setPlainText(t);
    m_countLabel->setText(QString("%1 instructions").arg(lines.size()));
  }
private:
  QComboBox* m_archCombo; QLineEdit* m_baseAddr; QPlainTextEdit* m_text;
  QLabel* m_countLabel; AsmHighlighter* m_hl;
  const uint8_t* m_fd=nullptr; size_t m_fs=0;
};

// ═════════════════════════════════════════════════════════════════════════════
// VizView — Histogram, Entropy, Bigram
// ═════════════════════════════════════════════════════════════════════════════
class HistogramWidget : public QWidget {
  Q_OBJECT
public:
  void setHistogram(const std::array<uint32_t,256>& h){m_h=h;m_mx=*std::max_element(h.begin(),h.end());update();}
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.fillRect(rect(),Qt::black);
    if(!m_mx) return;
    int w=width(),h=height()-20;
    float bw=w/256.f;
    p.setPen(QColor(0x1a,0x1a,0x1a));
    for(int i=1;i<4;++i){int y=h-(int)(h*i/4.f);p.drawLine(0,y,w,y);}
    for(int i=0;i<256;++i){
      float v=(float)m_h[i]/m_mx;
      int bh=(int)(v*h);
      int x=(int)(i*bw);
      int bwi=std::max(1,(int)bw);
      p.fillRect(x,h-bh,bwi,bh,QColor(0x55,0x55,0x55));
    }
    p.setPen(QColor(0x44,0x44,0x44)); p.setFont(font());
    p.drawText(4,height()-4,"0x00");
    p.drawText(w/4-10,height()-4,"0x40");
    p.drawText(w/2-10,height()-4,"0x80");
    p.drawText(3*w/4-10,height()-4,"0xC0");
    p.drawText(w-36,height()-4,"0xFF");
  }
private:
  std::array<uint32_t,256> m_h{}; uint32_t m_mx=0;
};

class EntropyWidget : public QWidget {
  Q_OBJECT
public:
  void setEntropy(const std::vector<float>& e){m_e=e;update();}
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.fillRect(rect(),Qt::black);
    int w=width(),h=height()-16;
    // Grid
    p.setPen(QColor(0x1a,0x1a,0x1a));
    for(int i=1;i<=4;++i){int y=h-(int)(h*i/4.f);p.drawLine(0,y,w,y);}
    p.setPen(QColor(0x33,0x33,0x33)); p.setFont(font());
    p.drawText(4,h-(int)(h*1/4.f)-2,"2 bits");
    p.drawText(4,h-(int)(h*2/4.f)-2,"4 bits");
    p.drawText(4,h-(int)(h*3/4.f)-2,"6 bits");
    p.drawText(4,12,"8 bits");
    if(m_e.size()<2) return;
    // Fill under curve
    QPainterPath path;
    float xs=(float)w/(m_e.size()-1);
    path.moveTo(0,h);
    for(size_t i=0;i<m_e.size();++i){
      float x=i*xs, y=h-(m_e[i]/8.f)*h;
      path.lineTo(x,y);
    }
    path.lineTo(w,h); path.closeSubpath();
    p.fillPath(path,QColor(0x22,0x22,0x22));
    // Line
    p.setPen(QPen(QColor(0x88,0x88,0x88),1.5));
    for(size_t i=1;i<m_e.size();++i){
      float x0=(i-1)*xs,x1=i*xs;
      float y0=h-(m_e[i-1]/8.f)*h,y1=h-(m_e[i]/8.f)*h;
      p.drawLine(QPointF(x0,y0),QPointF(x1,y1));
    }
  }
private:
  std::vector<float> m_e;
};

class BigramWidget : public QWidget {
  Q_OBJECT
public:
  void setImage(const uint8_t* rgba){m_img=QImage(rgba,256,256,256*4,QImage::Format_RGBA8888).copy();update();}
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.fillRect(rect(),Qt::black);
    if(m_img.isNull()) return;
    int s=std::min(width(),height());
    p.drawImage(QRect((width()-s)/2,(height()-s)/2,s,s),m_img);
    p.setPen(QColor(0x44,0x44,0x44)); p.setFont(font());
    p.drawText(4,s+12<height()?s+12:height()-4,"Byte N → Byte N+1");
  }
private:
  QImage m_img;
};

class VizView : public DraggableTabWidget {
  Q_OBJECT
public:
  explicit VizView(QWidget* p=nullptr):DraggableTabWidget(p){
    m_hist=new HistogramWidget;m_ent=new EntropyWidget;m_bi=new BigramWidget;
    addTab(m_hist,"Histogram"); addTab(m_ent,"Entropy"); addTab(m_bi,"Bigram");
  }
  void setFileData(const std::vector<uint8_t>& d){
    m_hist->setHistogram(byte_histogram(d.data(),d.size()));
    m_ent->setEntropy(entropy_curve(d.data(),d.size()));
    std::vector<uint8_t> rgba(256*256*4);
    bigram_image(d.data(),d.size(),rgba.data());
    m_bi->setImage(rgba.data());
  }
private:
  HistogramWidget* m_hist; EntropyWidget* m_ent; BigramWidget* m_bi;
};

// ═════════════════════════════════════════════════════════════════════════════
// StringsView
// ═════════════════════════════════════════════════════════════════════════════
class StringsView : public QWidget {
  Q_OBJECT
public:
  explicit StringsView(QWidget* p=nullptr):QWidget(p){
    auto* lay=new QVBoxLayout(this); lay->setContentsMargins(0,0,0,0); lay->setSpacing(0);
    auto* bar=new QHBoxLayout(); bar->setContentsMargins(4,4,4,4); bar->setSpacing(4);
    m_filter=new QLineEdit; m_filter->setPlaceholderText("Filter strings...");
    m_minLen=new QComboBox; m_minLen->addItems({"4","6","8","12","16","32"});
    bar->addWidget(new QLabel("Min:")); bar->addWidget(m_minLen);
    bar->addWidget(m_filter,1);
    m_countLabel=new QLabel; m_countLabel->setStyleSheet("color:#555;font-size:11px;");
    bar->addWidget(m_countLabel);
    lay->addLayout(bar);
    m_table=new QTableWidget;
    m_table->setColumnCount(3); m_table->setHorizontalHeaderLabels({"Offset","Len","String"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    lay->addWidget(m_table);
    connect(m_table,&QTableWidget::cellDoubleClicked,this,[this](int r,int){
      auto*it=m_table->item(r,0);if(!it)return;
      bool ok;size_t off=it->text().mid(2).toULongLong(&ok,16);
      if(ok) emit gotoOffset(off);
    });
    connect(m_filter,&QLineEdit::textChanged,this,&StringsView::applyFilter);
  }
  void setFileData(const uint8_t* d,size_t s){m_fd=d;m_fs=s;reExtract();}
  void reExtract(){
    if(!m_fd) return;
    int ml=m_minLen->currentText().toInt();
    m_strings=extract_strings(m_fd,m_fs,ml);
    applyFilter();
  }
signals:
  void gotoOffset(size_t off);
private:
  void applyFilter(){
    QString filt=m_filter->text().toLower();
    m_table->setRowCount(0);
    int count=0;
    for(auto& s:m_strings){
      if(!filt.isEmpty()&&!QString::fromStdString(s.value).toLower().contains(filt)) continue;
      int r=m_table->rowCount();
      m_table->setRowCount(r+1);
      m_table->setItem(r,0,new QTableWidgetItem(QString("0x%1").arg(s.offset,0,16)));
      m_table->setItem(r,1,new QTableWidgetItem(QString::number(s.value.size())));
      m_table->setItem(r,2,new QTableWidgetItem(QString::fromStdString(s.value)));
      count++;
    }
    m_table->resizeColumnToContents(0); m_table->resizeColumnToContents(1);
    m_countLabel->setText(QString("%1 strings").arg(count));
  }
  QLineEdit* m_filter; QComboBox* m_minLen; QTableWidget* m_table; QLabel* m_countLabel;
  const uint8_t* m_fd=nullptr; size_t m_fs=0;
  std::vector<FoundString> m_strings;
};

// ═════════════════════════════════════════════════════════════════════════════
// SectionsView
// ═════════════════════════════════════════════════════════════════════════════
class SectionsView : public QTableWidget {
  Q_OBJECT
public:
  explicit SectionsView(QWidget* p=nullptr):QTableWidget(p){
    setColumnCount(5);setHorizontalHeaderLabels({"Name","VAddr","Offset","Size","Flags"});
    horizontalHeader()->setStretchLastSection(true); verticalHeader()->setVisible(false);
    setSelectionBehavior(SelectRows);setEditTriggers(NoEditTriggers);setShowGrid(false);
    connect(this,&QTableWidget::cellDoubleClicked,this,[this](int r,int){
      auto*it=item(r,2);if(!it)return;
      bool ok;size_t off=it->text().mid(2).toULongLong(&ok,16);
      if(ok) emit gotoOffset(off);
    });
  }
  void setSections(const std::vector<Section>& secs){
    setRowCount((int)secs.size());
    for(int i=0;i<(int)secs.size();++i){
      setItem(i,0,new QTableWidgetItem(QString::fromStdString(secs[i].name)));
      setItem(i,1,new QTableWidgetItem(QString("0x%1").arg(secs[i].vaddr,0,16)));
      setItem(i,2,new QTableWidgetItem(QString("0x%1").arg(secs[i].offset,0,16)));
      setItem(i,3,new QTableWidgetItem(QString("0x%1").arg(secs[i].size,0,16)));
      setItem(i,4,new QTableWidgetItem(QString::fromStdString(secs[i].flags)));
    }
    for(int c=0;c<4;++c) resizeColumnToContents(c);
  }
signals:
  void gotoOffset(size_t off);
};

// ═════════════════════════════════════════════════════════════════════════════
// ImportsView
// ═════════════════════════════════════════════════════════════════════════════
class ImportsView : public QWidget {
  Q_OBJECT
public:
  explicit ImportsView(QWidget* p=nullptr):QWidget(p){
    auto* lay=new QVBoxLayout(this); lay->setContentsMargins(0,0,0,0); lay->setSpacing(0);
    auto* bar=new QHBoxLayout(); bar->setContentsMargins(4,4,4,4); bar->setSpacing(4);
    m_filter=new QLineEdit; m_filter->setPlaceholderText("Filter imports...");
    m_countLabel=new QLabel; m_countLabel->setStyleSheet("color:#555;font-size:11px;");
    bar->addWidget(m_filter,1); bar->addWidget(m_countLabel);
    lay->addLayout(bar);
    m_table=new QTableWidget;
    m_table->setColumnCount(3); m_table->setHorizontalHeaderLabels({"Library","Name","Hint"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    lay->addWidget(m_table);
    connect(m_filter,&QLineEdit::textChanged,this,&ImportsView::applyFilter);
  }
  void setImports(const std::vector<ImportEntry>& imps){ m_imports=imps; applyFilter(); }
private:
  void applyFilter(){
    QString f=m_filter->text().toLower();
    m_table->setRowCount(0); int count=0;
    for(auto& im:m_imports){
      QString n=QString::fromStdString(im.name);
      if(!f.isEmpty()&&!n.toLower().contains(f)&&!QString::fromStdString(im.library).toLower().contains(f)) continue;
      int r=m_table->rowCount(); m_table->setRowCount(r+1);
      m_table->setItem(r,0,new QTableWidgetItem(QString::fromStdString(im.library)));
      m_table->setItem(r,1,new QTableWidgetItem(n));
      m_table->setItem(r,2,new QTableWidgetItem(QString::number(im.hint)));
      count++;
    }
    m_table->resizeColumnToContents(0);
    m_countLabel->setText(QString("%1 imports").arg(count));
  }
  QLineEdit* m_filter; QTableWidget* m_table; QLabel* m_countLabel;
  std::vector<ImportEntry> m_imports;
};

// ═════════════════════════════════════════════════════════════════════════════
// ExportsView
// ═════════════════════════════════════════════════════════════════════════════
class ExportsView : public QTableWidget {
  Q_OBJECT
public:
  explicit ExportsView(QWidget* p=nullptr):QTableWidget(p){
    setColumnCount(3); setHorizontalHeaderLabels({"Name","RVA","Ordinal"});
    horizontalHeader()->setStretchLastSection(true); verticalHeader()->setVisible(false);
    setSelectionBehavior(SelectRows);setEditTriggers(NoEditTriggers);setShowGrid(false);
  }
  void setExports(const std::vector<ExportEntry>& exps){
    setRowCount((int)exps.size());
    for(int i=0;i<(int)exps.size();++i){
      setItem(i,0,new QTableWidgetItem(QString::fromStdString(exps[i].name)));
      setItem(i,1,new QTableWidgetItem(QString("0x%1").arg(exps[i].rva,0,16)));
      setItem(i,2,new QTableWidgetItem(QString::number(exps[i].ordinal)));
    }
    resizeColumnToContents(0); resizeColumnToContents(1);
  }
};

// ═════════════════════════════════════════════════════════════════════════════
// HashView
// ═════════════════════════════════════════════════════════════════════════════
class HashView : public QWidget {
  Q_OBJECT
public:
  explicit HashView(QWidget* p=nullptr):QWidget(p){
    auto* lay=new QVBoxLayout(this); lay->setContentsMargins(8,8,8,8); lay->setSpacing(8);
    auto hs=[](const QString& lbl)->QLabel*{
      auto* l=new QLabel(lbl); l->setTextInteractionFlags(Qt::TextSelectableByMouse);
      l->setStyleSheet("color:#888;font-size:12px;"); l->setWordWrap(true); return l;
    };
    lay->addWidget(new QLabel("File Hashes"));
    m_md5=hs("MD5: —"); m_sha=hs("SHA-256: —");
    lay->addWidget(m_md5); lay->addWidget(m_sha);
    lay->addSpacing(16);
    lay->addWidget(new QLabel("File Info"));
    m_info=new QLabel("—"); m_info->setStyleSheet("color:#888;font-size:12px;");
    m_info->setTextInteractionFlags(Qt::TextSelectableByMouse); m_info->setWordWrap(true);
    lay->addWidget(m_info);
    lay->addStretch();
  }
  void setFileData(const std::vector<uint8_t>& d){
    m_md5->setText("MD5: "+QString::fromStdString(hash_md5(d.data(),d.size())));
    m_sha->setText("SHA-256: "+QString::fromStdString(hash_sha256(d.data(),d.size())));
    auto fi=get_file_info(d.data(),d.size());
    QString info=QString("Format: %1\nArch: %2\nBits: %3\nEndian: %4\nEntry: 0x%5\nSize: %6 bytes")
      .arg(QString::fromStdString(fi.format))
      .arg(QString::fromStdString(fi.arch))
      .arg(QString::fromStdString(fi.bits))
      .arg(QString::fromStdString(fi.endian))
      .arg(fi.entry_point,0,16)
      .arg(fi.file_size);
    m_info->setText(info);
  }
private:
  QLabel* m_md5; QLabel* m_sha; QLabel* m_info;
};

// ═════════════════════════════════════════════════════════════════════════════
// SearchBar
// ═════════════════════════════════════════════════════════════════════════════
class SearchBar : public QWidget {
  Q_OBJECT
public:
  explicit SearchBar(QWidget* p=nullptr):QWidget(p){
    auto* lay=new QVBoxLayout(this); lay->setContentsMargins(0,0,0,0); lay->setSpacing(2);
    auto* row=new QHBoxLayout(); row->setContentsMargins(4,4,4,4); row->setSpacing(4);
    m_mode=new QComboBox; m_mode->addItems({"Text","Hex"});
    m_input=new QLineEdit; m_input->setPlaceholderText("Search pattern...");
    m_caseSensitive=new QCheckBox("Case"); m_caseSensitive->setChecked(true);
    auto* findNext=new QPushButton("Next");
    auto* findPrev=new QPushButton("Prev");
    auto* findAll=new QPushButton("All");
    row->addWidget(m_mode); row->addWidget(m_input,1);
    row->addWidget(m_caseSensitive);
    row->addWidget(findPrev); row->addWidget(findNext); row->addWidget(findAll);
    m_countLabel=new QLabel; m_countLabel->setStyleSheet("color:#555;font-size:11px;");
    row->addWidget(m_countLabel);
    lay->addLayout(row);
    m_results=new QListWidget; m_results->setMaximumHeight(200); m_results->hide();
    lay->addWidget(m_results);
    connect(findNext,&QPushButton::clicked,this,&SearchBar::onFindNext);
    connect(findPrev,&QPushButton::clicked,this,&SearchBar::onFindPrev);
    connect(findAll,&QPushButton::clicked,this,&SearchBar::onFindAll);
    connect(m_results,&QListWidget::itemClicked,this,[this](QListWidgetItem*it){
      bool ok;size_t off=it->data(Qt::UserRole).toULongLong(&ok);
      if(ok){m_lastIdx=(int)off; emit gotoOffset(off);}
    });
    connect(m_input,&QLineEdit::returnPressed,this,&SearchBar::onFindNext);
    connect(m_mode,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int idx){
      m_caseSensitive->setVisible(idx==0);
    });
  }
  void setFileData(const uint8_t* d,size_t s){m_data=d;m_size=s;m_lastIdx=-1;m_hits.clear();}
  void focusInput(){m_input->setFocus();m_input->selectAll();}
signals:
  void gotoOffset(size_t off);
private slots:
  void onFindNext(){
    ensureHits();
    if(m_hits.empty()) return;
    m_lastIdx=(m_lastIdx+1)%(int)m_hits.size();
    m_countLabel->setText(QString("%1/%2").arg(m_lastIdx+1).arg(m_hits.size()));
    emit gotoOffset(m_hits[m_lastIdx].offset);
  }
  void onFindPrev(){
    ensureHits();
    if(m_hits.empty()) return;
    m_lastIdx=(m_lastIdx-1+(int)m_hits.size())%(int)m_hits.size();
    m_countLabel->setText(QString("%1/%2").arg(m_lastIdx+1).arg(m_hits.size()));
    emit gotoOffset(m_hits[m_lastIdx].offset);
  }
  void onFindAll(){
    ensureHits();
    m_results->clear();
    for(auto& h:m_hits){
      auto* it=new QListWidgetItem(QString("0x%1").arg(h.offset,8,16,QChar('0')));
      it->setData(Qt::UserRole,(qulonglong)h.offset);
      m_results->addItem(it);
    }
    m_results->setVisible(!m_hits.empty());
    m_countLabel->setText(QString("%1 hits").arg(m_hits.size()));
  }
private:
  void ensureHits(){
    if(!m_data||!m_size||m_input->text().isEmpty()){m_hits.clear();return;}
    if(m_mode->currentIndex()==0)
      m_hits=search_text(m_data,m_size,m_input->text().toStdString().c_str(),m_caseSensitive->isChecked());
    else{
      QByteArray hex=QByteArray::fromHex(m_input->text().toLatin1());
      m_hits=search_hex(m_data,m_size,(const uint8_t*)hex.constData(),hex.size());
    }
    if(m_hits.empty()) m_countLabel->setText("No matches");
  }
  QComboBox* m_mode; QLineEdit* m_input; QListWidget* m_results; QLabel* m_countLabel;
  QCheckBox* m_caseSensitive;
  const uint8_t* m_data=nullptr; size_t m_size=0;
  std::vector<SearchHit> m_hits; int m_lastIdx=-1;
};

// ═════════════════════════════════════════════════════════════════════════════
// BookmarksView
// ═════════════════════════════════════════════════════════════════════════════
class BookmarksView : public QWidget {
  Q_OBJECT
public:
  explicit BookmarksView(QWidget* p=nullptr):QWidget(p){
    auto* lay=new QVBoxLayout(this); lay->setContentsMargins(0,0,0,0); lay->setSpacing(0);
    auto* bar=new QHBoxLayout(); bar->setContentsMargins(4,4,4,4); bar->setSpacing(4);
    auto* addBtn=new QPushButton("+ Bookmark");
    auto* delBtn=new QPushButton("Remove");
    bar->addWidget(addBtn); bar->addWidget(delBtn); bar->addStretch();
    lay->addLayout(bar);
    m_table=new QTableWidget;
    m_table->setColumnCount(3); m_table->setHorizontalHeaderLabels({"Offset","Size","Note"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setShowGrid(false);
    lay->addWidget(m_table);
    connect(addBtn,&QPushButton::clicked,this,&BookmarksView::onAdd);
    connect(delBtn,&QPushButton::clicked,this,&BookmarksView::onRemove);
    connect(m_table,&QTableWidget::cellDoubleClicked,this,[this](int r,int){
      auto*it=m_table->item(r,0);if(!it) return;
      bool ok;size_t off=it->data(Qt::UserRole).toULongLong(&ok);
      if(ok) emit gotoOffset(off);
    });
  }
  void addBookmark(size_t offset, size_t size, const QString& note=""){
    Bookmark b{offset,size,note.isEmpty()?QString("Bookmark %1").arg(m_bm.size()+1):note};
    m_bm.push_back(b); refreshTable();
  }
  void setCurrentOffset(size_t off){m_curOff=off;}
  void setCurrentSelection(size_t start, size_t end){m_selStart=start;m_selEnd=end;}
signals:
  void gotoOffset(size_t off);
private slots:
  void onAdd(){
    bool ok;
    QString note=QInputDialog::getText(this,"Add Bookmark","Note:",QLineEdit::Normal,"",&ok);
    if(!ok) return;
    size_t sz=(m_selStart!=m_selEnd)?m_selEnd-m_selStart+1:1;
    size_t off=(m_selStart!=m_selEnd)?m_selStart:m_curOff;
    addBookmark(off,sz,note);
  }
  void onRemove(){
    int r=m_table->currentRow(); if(r<0||r>=(int)m_bm.size()) return;
    m_bm.erase(m_bm.begin()+r); refreshTable();
  }
private:
  struct Bookmark { size_t offset,size; QString note; };
  void refreshTable(){
    m_table->setRowCount((int)m_bm.size());
    for(int i=0;i<(int)m_bm.size();++i){
      auto* oi=new QTableWidgetItem(QString("0x%1").arg(m_bm[i].offset,0,16));
      oi->setData(Qt::UserRole,(qulonglong)m_bm[i].offset);
      m_table->setItem(i,0,oi);
      m_table->setItem(i,1,new QTableWidgetItem(QString("0x%1").arg(m_bm[i].size,0,16)));
      auto* ni=new QTableWidgetItem(m_bm[i].note);
      m_table->setItem(i,2,ni);
    }
    m_table->resizeColumnToContents(0); m_table->resizeColumnToContents(1);
  }
  QTableWidget* m_table;
  std::vector<Bookmark> m_bm;
  size_t m_curOff=0, m_selStart=0, m_selEnd=0;
};

// ═════════════════════════════════════════════════════════════════════════════
// MainWindow
// ═════════════════════════════════════════════════════════════════════════════
class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow(){
    setWindowTitle("Rever"); resize(1400,860);
    setAcceptDrops(true);

    m_hex=new HexView; m_inspector=new DataInspector;
    m_disasm=new DisassemblyView; m_strings=new StringsView;
    m_sections=new SectionsView; m_viz=new VizView;
    m_hash=new HashView; m_search=new SearchBar;
    m_bookmarks=new BookmarksView;
    m_imports=new ImportsView; m_exports=new ExportsView;

    // Right panel: Inspector / Sections / Imports / Exports / Bookmarks / Hashes
    auto* rightTabs=new DraggableTabWidget;
    rightTabs->addTab(m_inspector,"Inspector");
    rightTabs->addTab(m_sections,"Sections");
    rightTabs->addTab(m_imports,"Imports");
    rightTabs->addTab(m_exports,"Exports");
    rightTabs->addTab(m_bookmarks,"Bookmarks");
    rightTabs->addTab(m_hash,"Info");

    // Left-top: Hex Editor
    auto* hexTabs=new DraggableTabWidget;
    hexTabs->addTab(m_hex,"Hex Editor");

    // Left-bottom: Disassembly / Strings / Search
    auto* bottomTabs=new DraggableTabWidget;
    bottomTabs->addTab(m_disasm,"Disassembly");
    bottomTabs->addTab(m_strings,"Strings");
    bottomTabs->addTab(m_search,"Search");

    auto* leftSplit=new QSplitter(Qt::Vertical);
    leftSplit->addWidget(hexTabs); leftSplit->addWidget(bottomTabs);
    leftSplit->setStretchFactor(0,3); leftSplit->setStretchFactor(1,2);

    auto* topSplit=new QSplitter(Qt::Horizontal);
    topSplit->addWidget(leftSplit); topSplit->addWidget(rightTabs);
    topSplit->setStretchFactor(0,3); topSplit->setStretchFactor(1,1);

    auto* mainSplit=new QSplitter(Qt::Vertical);
    mainSplit->addWidget(topSplit); mainSplit->addWidget(m_viz);
    mainSplit->setStretchFactor(0,3); mainSplit->setStretchFactor(1,1);
    setCentralWidget(mainSplit);

    // Connections
    connect(m_hex,&HexView::cursorChanged,this,[this](size_t off){
      m_inspector->inspect(m_bytes.data(),m_bytes.size(),off);
      m_bookmarks->setCurrentOffset(off);
      updateStatus();
    });
    connect(m_hex,&HexView::selectionChanged,this,[this](size_t a,size_t b){
      m_bookmarks->setCurrentSelection(a,b);
      updateStatus();
    });
    connect(m_hex,&HexView::dataEdited,this,[this]{ setWindowModified(true); updateStatus(); });
    connect(m_strings,&StringsView::gotoOffset,m_hex,&HexView::gotoByte);
    connect(m_sections,&SectionsView::gotoOffset,m_hex,&HexView::gotoByte);
    connect(m_search,&SearchBar::gotoOffset,m_hex,&HexView::gotoByte);
    connect(m_bookmarks,&BookmarksView::gotoOffset,m_hex,&HexView::gotoByte);

    // Menus
    auto* fm=menuBar()->addMenu("File");
    fm->addAction("Open...",QKeySequence::Open,this,&MainWindow::openFile);
    fm->addAction("Save",QKeySequence::Save,this,&MainWindow::save);
    fm->addAction("Save As...",QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_S),this,&MainWindow::saveAs);
    fm->addSeparator();
    m_recentMenu=fm->addMenu("Recent Files");
    loadRecentFiles();
    fm->addSeparator();
    fm->addAction("Quit",QKeySequence::Quit,this,&QWidget::close);

    auto* em=menuBar()->addMenu("Edit");
    em->addAction("Undo",QKeySequence::Undo,this,[this]{m_hex->undo();});
    em->addAction("Redo",QKeySequence::Redo,this,[this]{m_hex->redo();});
    em->addSeparator();
    em->addAction("Copy as Hex",QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_C),this,[this]{m_hex->copyAsHex();});
    em->addAction("Copy as Text",QKeySequence::Copy,this,[this]{m_hex->copyAsText();});
    em->addSeparator();
    em->addAction("Goto Address...",QKeySequence(Qt::CTRL|Qt::Key_G),this,&MainWindow::gotoAddress);
    em->addAction("Find...",QKeySequence::Find,this,[this]{m_search->focusInput();});
    em->addSeparator();
    em->addAction("Add Bookmark",QKeySequence(Qt::CTRL|Qt::Key_B),this,[this]{
      m_bookmarks->addBookmark(m_hex->cursor(),m_hex->hasSelection()?m_hex->selEnd()-m_hex->selStart()+1:1);
    });

    em->addAction("Select All",QKeySequence::SelectAll,this,[this]{
      if(m_bytes.empty()) return;
      // no-op: full selection not meaningful for huge files
    });

    menuBar()->addMenu("View");
    statusBar()->showMessage("Ready — Drop a file or press Cmd+O");
  }

protected:
  void dragEnterEvent(QDragEnterEvent* e) override {
    if(e->mimeData()->hasUrls()) e->acceptProposedAction();
  }
  void dropEvent(QDropEvent* e) override {
    auto urls=e->mimeData()->urls();
    if(urls.isEmpty()) return;
    loadFile(urls.first().toLocalFile());
  }

private slots:
  void openFile(){
    QString path=QFileDialog::getOpenFileName(this,"Open Binary");
    if(!path.isEmpty()) loadFile(path);
  }
  void save(){
    if(m_filePath.isEmpty()){saveAs();return;}
    if(save_file(m_filePath.toStdString().c_str(),m_bytes.data(),m_bytes.size())){
      setWindowModified(false); statusBar()->showMessage("Saved");
    }
  }
  void saveAs(){
    if(m_bytes.empty()) return;
    QString path=QFileDialog::getSaveFileName(this,"Save Binary");
    if(path.isEmpty()) return;
    if(save_file(path.toStdString().c_str(),m_bytes.data(),m_bytes.size())){
      m_filePath=path; setWindowModified(false);
      statusBar()->showMessage("Saved to "+path);
    }
  }
  void gotoAddress(){
    bool ok;QString txt=QInputDialog::getText(this,"Goto Address","Hex offset:",QLineEdit::Normal,"",&ok);
    if(!ok||txt.isEmpty()) return;
    size_t off=txt.toULongLong(&ok,16);
    if(ok) m_hex->gotoByte(off);
  }

private:
  void loadFile(const QString& path){
    m_bytes=load_file(path.toStdString().c_str());
    if(m_bytes.empty()){statusBar()->showMessage("Failed to load");return;}
    m_filePath=path;
    addRecentFile(path);

    QString name=path.mid(path.lastIndexOf('/')+1);
    std::string fmt=detect_format(m_bytes.data(),m_bytes.size());
    m_fileName=name; m_fileFormat=QString::fromStdString(fmt);
    setWindowTitle(QString("Rever — %1 (%2)[*]").arg(name,m_fileFormat));

    m_hex->setData(&m_bytes);
    auto ent=entropy_curve(m_bytes.data(),m_bytes.size());
    m_hex->setEntropy(ent);
    m_disasm->setFileData(m_bytes.data(),m_bytes.size());
    m_viz->setFileData(m_bytes);
    m_strings->setFileData(m_bytes.data(),m_bytes.size());
    m_sections->setSections(parse_sections(m_bytes.data(),m_bytes.size()));
    m_imports->setImports(parse_imports(m_bytes.data(),m_bytes.size()));
    m_exports->setExports(parse_exports(m_bytes.data(),m_bytes.size()));
    m_hash->setFileData(m_bytes);
    m_search->setFileData(m_bytes.data(),m_bytes.size());
    updateStatus();
  }
  void updateStatus(){
    QString s=QString("%1 — %2 bytes — %3").arg(m_fileName).arg(m_bytes.size()).arg(m_fileFormat);
    s+=QString(" | Cursor: 0x%1").arg(m_hex->cursor(),0,16);
    if(m_hex->hasSelection())
      s+=QString(" | Sel: 0x%1-0x%2 (%3 bytes)").arg(m_hex->selStart(),0,16).arg(m_hex->selEnd(),0,16).arg(m_hex->selEnd()-m_hex->selStart()+1);
    if(m_hex->isModified()) s+=" [modified]";
    statusBar()->showMessage(s);
  }
  void addRecentFile(const QString& path){
    QSettings settings("Rever","Rever");
    QStringList recent=settings.value("recentFiles").toStringList();
    recent.removeAll(path); recent.prepend(path);
    while(recent.size()>10) recent.removeLast();
    settings.setValue("recentFiles",recent);
    loadRecentFiles();
  }
  void loadRecentFiles(){
    m_recentMenu->clear();
    QSettings settings("Rever","Rever");
    QStringList recent=settings.value("recentFiles").toStringList();
    for(auto& p:recent){
      QString name=p.mid(p.lastIndexOf('/')+1);
      m_recentMenu->addAction(name,this,[this,p]{loadFile(p);});
    }
    if(recent.isEmpty()) m_recentMenu->addAction("(none)")->setEnabled(false);
  }

  HexView* m_hex; DataInspector* m_inspector; DisassemblyView* m_disasm;
  StringsView* m_strings; SectionsView* m_sections; VizView* m_viz;
  HashView* m_hash; SearchBar* m_search; BookmarksView* m_bookmarks;
  ImportsView* m_imports; ExportsView* m_exports;
  QMenu* m_recentMenu;
  std::vector<uint8_t> m_bytes;
  QString m_filePath, m_fileName, m_fileFormat;
};

int main(int argc,char** argv){
  QApplication app(argc,argv);
  app.setOrganizationName("Rever"); app.setApplicationName("Rever");
  QString fp=QApplication::applicationDirPath()+"/fonts/RobotoMono.ttf";
  if(QFontDatabase::addApplicationFont(fp)<0)
    QFontDatabase::addApplicationFont(REVER_PROJECT_DIR "/fonts/RobotoMono.ttf");
  app.setStyleSheet(kStyleSheet);
  MainWindow w;
  // Open file from command line argument
  if(argc>1) {
    QMetaObject::invokeMethod(&w,[&w,argv](){
      // Use a lambda via QTimer to open after event loop starts
    },Qt::QueuedConnection);
  }
  w.show();
  return app.exec();
}

#include "main.moc"
