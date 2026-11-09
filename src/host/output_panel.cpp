#include "host/output_panel.h"

#include <QFontDatabase>
#include <QScrollBar>

namespace vsglab::host {

OutputPanel::OutputPanel(QWidget* parent) : QPlainTextEdit(parent) {
    setReadOnly(true);
    setMaximumBlockCount(5000);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

void OutputPanel::appendMessage(LogBridge::Level level, const QString& text) {
    QString color;
    switch (level) {
    case LogBridge::Level::Debug: color = QStringLiteral("#808080"); break;
    case LogBridge::Level::Info: break;
    case LogBridge::Level::Warn: color = QStringLiteral("#c08000"); break;
    case LogBridge::Level::Error:
    case LogBridge::Level::Fatal: color = QStringLiteral("#d03030"); break;
    }

    QString html = text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    if (!color.isEmpty())
        html = QStringLiteral("<span style=\"color:%1\">%2</span>").arg(color, html);

    const bool atBottom = verticalScrollBar()->value() == verticalScrollBar()->maximum();
    appendHtml(html);
    if (atBottom)
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

}  // namespace vsglab::host
