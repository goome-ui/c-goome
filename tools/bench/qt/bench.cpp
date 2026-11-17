#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>
#include <chrono>
#include <cstdio>
#include <vector>

struct Row {
    QLabel *l;
    QCheckBox *c;
    QSlider *s;
};

static double now_ms()
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void report(const char *scene, int frames, double ms)
{
    std::printf("{\"lib\":\"qt\",\"scene\":\"%s\",\"frames\":%d,\"ms_per_frame\":%.5f}\n", scene, frames, ms / frames);
    std::fflush(stdout);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const int warm = 100, n = 2000;

    {
        QWidget w;
        auto *lay = new QVBoxLayout(&w);
        lay->addWidget(new QLabel("hello world"));
        lay->addWidget(new QPushButton("Click me"));
        auto *lab = new QLabel("clicks: 0");
        lay->addWidget(lab);
        w.resize(640, 400);
        w.show();
        QImage img(w.size(), QImage::Format_ARGB32_Premultiplied);
        double t = 0;
        for (int i = 0; i < warm + n; i++) {
            if (i == warm) t = now_ms();
            lab->setText(QString("clicks: %1").arg(i));
            img.fill(Qt::white);
            w.render(&img);
        }
        report("hello", n, now_ms() - t);
    }

    {
        QWidget w;
        auto *lay = new QVBoxLayout(&w);
        lay->setSpacing(0);
        lay->setContentsMargins(0, 0, 0, 0);
        std::vector<Row> rows;
        for (int i = 0; i < 50; i++) {
            auto *h = new QHBoxLayout;
            Row r;
            r.l = new QLabel(QString("row %1").arg(i));
            r.c = new QCheckBox("On");
            r.s = new QSlider(Qt::Horizontal);
            r.s->setRange(0, 1000);
            auto *b = new QPushButton("Open");
            auto *vl = new QLabel("Value");
            h->addWidget(r.l);
            h->addWidget(b);
            h->addWidget(r.c);
            h->addWidget(vl);
            h->addWidget(r.s, 1);
            lay->addLayout(h);
            rows.push_back(r);
        }
        w.resize(960, 640);
        w.show();
        QImage img(w.size(), QImage::Format_ARGB32_Premultiplied);
        double t = 0;
        for (int i = 0; i < warm + n; i++) {
            if (i == warm) t = now_ms();
            for (int k = 0; k < 50; k++) {
                rows[k].l->setText(QString("row %1").arg(k + (i & 1)));
                rows[k].c->setChecked(((i + k) & 1) == 0);
                rows[k].s->setValue((i + k) % 1000);
            }
            img.fill(Qt::white);
            w.render(&img);
        }
        report("dash", n, now_ms() - t);
    }
    return 0;
}
