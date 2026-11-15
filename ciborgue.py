import sys
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication
from app import CiborgueWorkbenchWindow


def main():
    app = QApplication(sys.argv)
    win = CiborgueWorkbenchWindow()
    win.setWindowFlag(Qt.WindowStaysOnTopHint, True)
    win.show()
    win.setWindowFlag(Qt.WindowStaysOnTopHint, False)
    win.show()
    win.raise_()
    win.activateWindow()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
