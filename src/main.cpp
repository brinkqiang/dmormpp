#include "dbng.hpp"
#include "mysql.hpp"
using namespace ormpp;

struct person
{
    int id;
    std::string name;
    int age;
};
REFLECTION(person, id, name, age)

int main()
{
    person p = { 1, "test1", 2 };
    person p1 = { 2, "test2", 3 };
    person p2 = { 3, "test3", 4 };
    std::vector<person> v{ p1, p2 };

    dbng<mysql> mysql;
    mysql.connect("127.0.0.1", "dbuser", "yourpwd", "testdb");
    mysql.create_datatable<person>();

    mysql.insert(p);
    mysql.insert(v);

    mysql.update(p);
    mysql.update(v);

    auto result = mysql.query<person>(); //vector<person>
    for (auto& person : result) {
        std::cout << person.id << " " << person.name << " " << person.age << std::endl;
    }

    mysql.delete_records<person>();

    //transaction
    mysql.begin();
    for (int i = 0; i < 10; ++i) {
        person s = { i, "tom", 19 };
        if (!mysql.insert(s)) {
            mysql.rollback();
            return -1;
        }
    }
    mysql.commit();
}