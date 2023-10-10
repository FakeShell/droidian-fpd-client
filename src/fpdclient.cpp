// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2023 Droidian Project
//
// Authors:
// Bardia Moshiri <fakeshell@bardia.tech>

#include <QCoreApplication>
#include <QTextStream>
#include <QDebug>
#include <QTimer>
#include <cstdlib>
#include "fpdinterface.h"

void printHelp(bool isInteractive) {
    qDebug() << "Available commands:";
    qDebug() << "enroll <finger>: Start the enrollment process for the specified finger";
    qDebug() << "identify: Start the identification process";
    qDebug() << "remove <finger>: Remove the specified finger";
    qDebug() << "clear/cls: Clear all fingerprints";
    qDebug() << "list/ls: List all enrolled fingers";
    qDebug() << "help/-h/--help: Display this help message";
    if (isInteractive)
        qDebug() << "exit/q/quit: Exit the program";
}

void handleInput(FPDInterface &fpdInterface, const QString &input)
{
    if (input.startsWith("enroll ")) {
        QString finger = input.section(' ', 1).trimmed();
        qDebug() << "Enrolling finger:" << finger;
        fpdInterface.enroll(finger);
    } else if (input == "identify") {
        qDebug() << "Identifying...";
        fpdInterface.identify();
    } else if (input.startsWith("remove ") || input.startsWith("rm ")) {
        QString finger = input.section(' ', 1).trimmed();
        qDebug() << "Removing finger:" << finger;
        fpdInterface.remove(finger);
    } else if (input == "clear" || input == "cls") {
        qDebug() << "All fingerprints have been cleared.";
        fpdInterface.clear();
    } else if (input == "help" || input == "-h" || input == "--help") {
        printHelp(true);
    } else if (input == "list" || input == "ls") {
        QStringList fingers = fpdInterface.fingerprints();
        if (fingers.isEmpty())
            qDebug() << "No enrolled fingers.";
        else
            qDebug() << "Enrolled fingers: " << fingers.join(", ");
    } else if (input == "exit" || input == "q" || input == "quit") {
        qDebug() << "Exiting.";
        exit(0);
    } else {
        qDebug() << "Unknown command:" << input;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    FPDInterface fpdInterface;

    if (argc == 1) {
        printHelp(true);

        QStringList fingers = fpdInterface.fingerprints();
        if (fingers.isEmpty())
            qDebug() << "No enrolled fingers.";
        else
            qDebug() << "Enrolled fingers: " << fingers.join(", ");

        QObject::connect(&fpdInterface, &FPDInterface::connectionStateChanged, []() {
            qDebug() << "Connection state changed";
        });
        QObject::connect(&fpdInterface, &FPDInterface::fingerprintsChanged, []() {
            qDebug() << "Fingerprints changed";
        });
        QObject::connect(&fpdInterface, &FPDInterface::stateChanged, [](const QString &state) {
            qDebug() << "State changed:" << state;
        });
        QObject::connect(&fpdInterface, &FPDInterface::enrollProgressChanged, [](int progress) {
            qDebug() << "Enroll progress changed:" << progress;
        });
        QObject::connect(&fpdInterface, &FPDInterface::acquisitionInfo, [](const QString &info) {
            if(info != "FPACQUIRED_UNRECOGNIZED") // Ignore unwanted status message
                qDebug() << "Acquisition info:" << info;
        });
        QObject::connect(&fpdInterface, &FPDInterface::errorInfo, [](const QString &info) {
            qDebug() << "Error info:" << info;
        });
        QObject::connect(&fpdInterface, &FPDInterface::added, [](const QString &finger) {
            qDebug() << "Added finger:" << finger;
        });
        QObject::connect(&fpdInterface, &FPDInterface::removed, [](const QString &finger) {
            qDebug() << "Removed finger:" << finger;
        });
        QObject::connect(&fpdInterface, &FPDInterface::identified, [](const QString &finger) {
            qDebug() << "Identified finger:" << finger;
        });
        QObject::connect(&fpdInterface, &FPDInterface::aborted, []() {
            qDebug() << "Operation aborted";
        });
        QObject::connect(&fpdInterface, &FPDInterface::failed, []() {
            qDebug() << "Operation failed";
        });
        QObject::connect(&fpdInterface, &FPDInterface::verified, []() {
            qDebug() << "Verification successful";
        });

        QObject::connect(&fpdInterface, &FPDInterface::enrollProgressChanged,
                         [&fpdInterface](int progress) {
                             if (progress == 100) {
                                 qDebug() << "Enrollment complete! You can now execute another command.";
                             }
                         });

        QTextStream qin(stdin);
        while (true) {
            QString line = qin.readLine();
            if (!line.isNull()) {
                handleInput(fpdInterface, line);
                if (line.startsWith("enroll ")) {
                    while (fpdInterface.enrollProgress() != 100) {
                        app.processEvents();
                    }
                }
            } else {
                break;
            }
        }
    } else if (argc >= 2) {
        QString command(argv[1]);

        if (command == "list" || command == "ls") {
            QStringList fingers = fpdInterface.fingerprints();
            if (fingers.isEmpty()) {
                return 1;
            } else {
                qDebug().noquote() << fingers.join(", ");
                return 0;
            }
        } else if (command == "enroll" && argc == 3) {
            QString finger(argv[2]);
            if (fpdInterface.fingerprints().contains(finger)) {
                qDebug() << "Fingerprint already enrolled:" << finger;
                return 1;
            }

            QObject::connect(&fpdInterface, &FPDInterface::enrollProgressChanged,
                [&app](int progress) {
                    if (progress == 100) {
                        app.quit();
                    } else {
                        qDebug() << progress;
                    }
                }
            );

            fpdInterface.enroll(finger);

            return app.exec();
        } else if ((command == "remove" || command == "rm") && argc == 3) {
            QString finger(argv[2]);
            if (fpdInterface.fingerprints().contains(finger)) {
                fpdInterface.remove(finger);
                return 0;
            } else {
                return 1;
            }
        } else if (command == "identify" && argc == 2) {
            QObject::connect(&fpdInterface, &FPDInterface::identified,
                [&app](const QString &finger) {
                    qDebug() << finger;
                    app.quit();
                }
            );

            fpdInterface.identify();

            return app.exec();
        } else if ((command == "clear" || command == "cls") && argc == 2) {
            fpdInterface.clear();
            return 0;
        } else if (command == "help" || command == "-h" || command == "--help") {
            printHelp(false);
            return 0;
        } else {
            qDebug() << "Unknown command or wrong number of arguments";
            printHelp(false);
            return 1;
        }
    }

    return app.exec();
}
