#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>
#include <QPaintEvent>
#include <cstdio>
#include <cstring>

class Win : public QWidget {
public:
    bool done = false;
protected:
    void paintEvent(QPaintEvent *e) override
    {
        QWidget::paintEvent(e);
        if (!done) {
            done = true;
            QTimer::singleShot(0, [] {
                std::printf("READY\n");
                std::fflush(stdout);
            });
        }
    }
};

int main(int argc, char **argv)
{
    if (argc > 1 && !std::strcmp(argv[1], "version")) {
        std::printf("%s\n", qVersion());
        return 0;
    }
    QApplication app(argc, argv);
    Win w;
    w.setWindowTitle("Hello");
    auto *lay = new QVBoxLayout(&w);
    lay->addWidget(new QLabel("hello world"));
    auto *btn = new QPushButton("Click me");
    auto *lab = new QLabel("clicks: 0");
    int clicks = 0;
    QObject::connect(btn, &QPushButton::clicked, [&] { lab->setText(QString("clicks: %1").arg(++clicks)); });
    lay->addWidget(btn);
    lay->addWidget(lab);
    w.resize(640, 400);
    w.show();
    return app.exec();
}
