// Copyright (C) 2018 Martin Ejdestig <marejde@gmail.com>
// SPDX-License-Identifier: GPL-2.0+

#include <QtCore>
#include <QtWidgets>

namespace
{
	class TestWidget : public QWidget
	{
	public:
		TestWidget()
		{
			timer.start(1000, this);
		}

		void timerEvent(QTimerEvent *event) override
		{
			if (event->timerId() == timer.timerId())
				update();
			else
				QWidget::timerEvent(event);
		}

		void keyPressEvent(QKeyEvent *event) override
		{
			switch (event->key()) {
			case Qt::Key_Escape:
			case Qt::Key_Q:
				QApplication::quit();
				break;
			default:
				QWidget::keyPressEvent(event);
				break;
			}
		}

		void paintEvent(QPaintEvent *) override
		{
			QPainter painter(this);

			paintVerticalSpectrum(painter);
			paintTime(painter);
		}

	private:
		void paintVerticalSpectrum(QPainter &painter) const
		{
			for (int y = 0; y < height(); y++) {
				qreal hue = qreal(y) / height();
				painter.fillRect(0, y, width(), 1, QColor::fromHsvF(hue, 1.0, 1.0));
			}
		}

		void paintTime(QPainter &painter) const
		{
			int size = height() / 4;
			int shadowOffset = size / 16;
			QString time = QTime::currentTime().toString("HH:mm:ss");

			painter.save();

			QFont font("monospace");
			font.setPixelSize(size);
			font.setBold(true);
			painter.setFont(font);

			painter.setPen(QPen(Qt::black));
			painter.drawText(rect().translated(shadowOffset, shadowOffset), Qt::AlignCenter, time);

			painter.setPen(QPen(Qt::white));
			painter.drawText(rect(), Qt::AlignCenter, time);

			painter.restore();
		}

		QBasicTimer timer;
	};
} // namespace

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	TestWidget widget;
	widget.show();

	return app.exec();
}
