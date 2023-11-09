#include <QCoreApplication>
#include <QtSerialPort/QSerialPort>
#include <QTextStream>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QSqlError>

class GPSData {
public:
    double latitude;
    double longitude;
    std::string time;
};

class QtDatabase : public QObject {
    Q_OBJECT

        QSqlDatabase db;
    QSerialPort serialport;
    std::string receivedData;

public:
    QtDatabase(QObject* parent = nullptr) : QObject(parent) {
        db = QSqlDatabase::addDatabase("QMYSQL");
        db.setHostName("192.168.65.9");
        db.setUserName("root");
        db.setPassword("root");
        db.setDatabaseName("trgps");

        if (db.open()) {
            qDebug() << "Connexion réussie à " << db.hostName();
            connect(&serialport, &QSerialPort::readyRead, this, &QtDatabase::onDataReceived);
            serialport.setBaudRate(4800);
            serialport.setDataBits(QSerialPort::Data8);
            serialport.setParity(QSerialPort::NoParity);
            serialport.setStopBits(QSerialPort::OneStop);
            serialport.setPortName("COM1");
            serialport.open(QIODevice::ReadWrite);
        }
        else {
            qDebug() << "La connexion a échouée !";
        }
    }

public slots:
    void onDataReceived() {
        QByteArray requestData = serialport.readAll();
        receivedData += requestData.toStdString();

        size_t found = receivedData.find("\r\n");
        if (found != std::string::npos) {
            std::string arduinoData = receivedData.substr(0, found);
            receivedData = receivedData.substr(found + 2);

            GPSData gpsData;
            if (ParseGPGGA(arduinoData, gpsData)) {
                qDebug() << "Time: " << QString::fromStdString(gpsData.time);
                qDebug() << "Latitude: " << gpsData.latitude << " degrees";
                qDebug() << "Longitude: " << gpsData.longitude << " degrees";

                // Insertion en base de données
                QSqlQuery query;
                query.prepare("INSERT INTO GPSdata (longitude, latitude, heure) VALUES (?, ?, ?)");
                query.addBindValue(gpsData.longitude);
                query.addBindValue(gpsData.latitude);
                query.addBindValue(gpsData.time.c_str());

                if (query.exec()) {
                    qDebug() << "Insertion réussie";
                }
                else {
                    qDebug() << "Echec insertion !";
                    qDebug() << query.lastError().text();
                }
            }
        }
    }

private:
    bool ParseGPGGA(const std::string& sentence, GPSData& data) {
       
            if (sentence.find("$GPGGA") == std::string::npos) {
                return false;
            }

            std::istringstream stream(sentence);
            std::string token;
            std::vector<std::string> tokens;

            while (std::getline(stream, token, ',')) {
                tokens.push_back(token);
            }

            if (tokens.size() < 15) {
                return false;
            }

            std::string time = tokens[1];
            if (time.empty()) {
                return false;
            }

            std::string latitude_str = tokens[2];
            if (latitude_str.empty()) {
                return false;
            }

            std::string longitude_str = tokens[4];
            if (longitude_str.empty()) {
                return false;
            }

            double latitude = std::stod(latitude_str);
            double longitude = std::stod(longitude_str);

            data.latitude = floor(latitude / 100) + fmod(latitude, 100) / 60;
            data.longitude = floor(longitude / 100) + fmod(longitude, 100) / 60;
            data.time = time;

       
        return true;  // Ajoute la logique de parsing ici
    }
};

int main(int argc, char* argv[]) {
    QCoreApplication a(argc, argv);
    QtDatabase database;
    return a.exec();
}

#include "main.moc"