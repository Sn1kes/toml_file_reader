#ifndef APP_HPP
#define APP_HPP

#include <include\toml.hpp>
#include <QObject>
#include <QCoreApplication>

class App : public QObject
{
    Q_OBJECT
private:
    QCoreApplication *qapp;

public:
    explicit App(QObject *parent = nullptr);

    template<typename T>
    T toml_find(const toml::basic_value<toml::discard_comments, std::unordered_map, std::vector>&, const char *);

public slots:
    void run();
};

//Wrapper for finding values in toml
template<typename T>
T App::toml_find(const toml::basic_value<toml::discard_comments, std::unordered_map, std::vector>& table, const char *val)
{
    T ret;
    try {
        ret = toml::find<T>(table, val);
    } catch(const std::out_of_range& e) {
        std::cerr << "Value \"" << val << "\" not found in config:\n" << e.what();
        qapp->exit(EXIT_FAILURE);
        std::exit(EXIT_FAILURE);
    } catch(const toml::type_error& e) {
        std::cerr << "Invalid type of value \"" << val << "\"\n" << e.what();
        qapp->exit(EXIT_FAILURE);
        std::exit(EXIT_FAILURE);
    }
    return ret;
}

#endif // APP_HPP
