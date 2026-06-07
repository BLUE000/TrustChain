#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include "TrustChainCore.hpp"
#include "TrustChainQt.hpp"

class DummyWindow : public QMainWindow {
public:
    DummyWindow() {
        setWindowTitle("TrustChain Dummy Application");
        resize(400, 300);

        QWidget* centralWidget = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(centralWidget);

        QLabel* label = new QLabel("This is a dummy application running with TrustChain integration.", this);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);

        setCentralWidget(centralWidget);
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // 1. Initialize TrustChain Core and verify token
    TrustChain::Core trustChainCore;
    TrustChain::AuthStatus status = trustChainCore.verifyToken();

    // 2. Terminate application if verification completely failed and process is dead
    if (status == TrustChain::AuthStatus::Terminated) {
        // Core::verifyToken() will normally call qFatal and exit before this,
        // but just in case we gracefully return if it didn't.
        return -1;
    }

    // 3. Create the main application window
    DummyWindow window;

    // 4. Apply UI Watermark based on TrustChain validation status
    TrustChain::QtHelper::applyWatermark(&window, status);

    window.show();

    return app.exec();
}
