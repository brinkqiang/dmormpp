//
// Created by qiyu on 10/28/17.
//
#include <string>
#include <vector>
#include <sqlite3.h>
#include "utility.hpp"

#ifndef ORM_SQLITE_HPP
#define ORM_SQLITE_HPP
namespace ormpp{
    class sqlite{
    public:
        ~sqlite(){
            disconnect();
        }

        template <typename... Args>
        bool connect(Args&&... args){
            auto r = sqlite3_open(std::forward<Args>(args)..., &handle_);
            return r == SQLITE_OK;
        }

        template <typename... Args>
        bool disconnect(Args&&... args){
            if(handle_!= nullptr){
                auto r = sqlite3_close(handle_);
                handle_ = nullptr;
                return r==SQLITE_OK;
            }

			return true;
        }

        template<typename T, typename... Args>
        constexpr bool create_datatable(Args&&... args){
//            std::string droptb = "DROP TABLE IF EXISTS ";
//            droptb += iguana::get_name<T>();
//            if (sqlite3_exec(handle_, droptb.data(), nullptr, nullptr, nullptr)!=SQLITE_OK) {
//                return false;
//            }

            std::string sql = generate_createtb_sql<T>(std::forward<Args>(args)...);
            if (sqlite3_exec(handle_, sql.data(), nullptr, nullptr, nullptr)!=SQLITE_OK) {
                return false;
            }

            return true;
        }

        template<typename T, typename... Args>
        constexpr int insert(const T& t,Args&&... args){
            std::string sql = auto_key_map_.empty()?generate_insert_sql<T>(false):generate_auto_insert_sql<T>(auto_key_map_, false);

            return insert_impl(false, sql, t, std::forward<Args>(args)...);
        }

        template<typename T, typename... Args>
        constexpr int insert(const std::vector<T>& t, Args&&... args){
            std::string sql = auto_key_map_.empty()?generate_insert_sql<T>(false):generate_auto_insert_sql<T>(auto_key_map_, false);

            return insert_impl(false, sql, t, std::forward<Args>(args)...);
        }

        template<typename T, typename... Args>
        constexpr int update(const T& t, Args&&... args) {
            std::string sql = generate_insert_sql<T>(true);

            return insert_impl(true, sql, t, std::forward<Args>(args)...);
        }

        template<typename T, typename... Args>
        constexpr int update(const std::vector<T>& t, Args&&... args){
            std::string sql = generate_insert_sql<T>(true);

            return insert_impl(true, sql, t, std::forward<Args>(args)...);
        }

        template<typename T, typename... Args>
        constexpr bool delete_records(Args&&... where_conditon){
            auto sql = generate_delete_sql<T>(std::forward<Args>(where_conditon)...);
            if (sqlite3_exec(handle_, sql.data(), nullptr, nullptr, nullptr)!=SQLITE_OK) {
                return false;
            }

            return true;
        }

        //restriction, all the args are string, the first is the where condition, rest are append conditions
        template<typename T, typename... Args>
        constexpr std::enable_if_t<iguana::is_reflection_v<T>, std::vector<T>> query(Args&&... args){
            std::string sql = generate_query_sql<T>(args...);
            constexpr auto SIZE = iguana::get_value<T>();

            int result = sqlite3_prepare_v2(handle_, sql.data(), (int)sql.size(), &stmt_, nullptr);
            if(result!=SQLITE_OK)
                return {};

            auto guard = guard_statment(stmt_);

            std::vector<T> v;
            while (true)
            {
                result = sqlite3_step(stmt_);
                if (result == SQLITE_DONE)
                    break;

                if (result != SQLITE_ROW)
                    break;

                T t = {};
                iguana::for_each(t, [this, &t](auto item, auto I)
                {
                    assign(t.*item, (int)decltype(I)::value);
                });

                v.push_back(std::move(t));
            }

            return v;
        }

        template<typename T, typename Arg, typename... Args>
        constexpr std::enable_if_t<!iguana::is_reflection_v<T>, std::vector<T>> query(const Arg& s, Args&&... args){
            static_assert(iguana::is_tuple<T>::value);
            constexpr auto SIZE = std::tuple_size_v<T>;

            std::string sql = s;
            constexpr auto Args_Size = sizeof...(Args);
            if(Args_Size!=0){
                if(Args_Size!=std::count(sql.begin(), sql.end(), '?'))
                    return {};

                sql = get_sql(sql, std::forward<Args>(args)...);
            }

            int result = sqlite3_prepare_v2(handle_, sql.data(), (int)sql.size(), &stmt_, nullptr);
            if(result!=SQLITE_OK)
                return {};

            auto guard = guard_statment(stmt_);

            std::vector<T> v;
            while (true)
            {
                result = sqlite3_step(stmt_);
                if (result == SQLITE_DONE)
                    break;

                if (result != SQLITE_ROW)
                    break;

                T tp = {};
                int index = 0;
                iguana::for_each(tp, [this, &index](auto& item, auto I)
                {
                    if constexpr(iguana::is_reflection_v<decltype(item)>){
                        std::remove_reference_t<decltype(item)> t = {};
                        iguana::for_each(t, [this, &index, &t](auto ele, auto i)
                        {
                            assign(t.*ele, index++);
                        });
                        item = std::move(t);
                    }else{
                        assign(item, index++);
                    }
                }, std::make_index_sequence<SIZE>{});

                v.push_back(std::move(tp));
            }

            return v;
        }

        //just support execute string sql without placeholders
        bool execute(const std::string& sql){
            if (sqlite3_exec(handle_, sql.data(), nullptr, nullptr, nullptr)!=SQLITE_OK) {
                return false;
            }

            return true;
        }

        //transaction
        bool begin(){
            if (sqlite3_exec(handle_, "BEGIN", nullptr, nullptr, nullptr)!=SQLITE_OK) {
                return false;
            }

            return true;
        }

        bool commit(){
            if (sqlite3_exec(handle_, "COMMIT", nullptr, nullptr, nullptr)!=SQLITE_OK) {
                return false;
            }

            return true;
        }

        bool rollback(){
            if (sqlite3_exec(handle_, "ROLLBACK", nullptr, nullptr, nullptr)!=SQLITE_OK) {
                return false;
            }

            return true;
        }

    private:
        template<typename T, typename... Args >
        std::string generate_createtb_sql(Args&&... args)
        {
            const auto type_name_arr = get_type_names<T>(DBType::sqlite);
            constexpr auto name = iguana::get_name<T>();
            std::string sql = std::string("CREATE TABLE IF NOT EXISTS ") + name.data()+"(";
            auto arr = iguana::get_array<T>();
            constexpr auto SIZE = sizeof... (Args);
            auto_key_map_[name.data()] = "";
            //auto_increment_key and key can't exist at the same time
			using U = std::tuple<std::decay_t <Args>...>;
            if constexpr (SIZE>0){
                //using U = std::tuple<std::decay_t <Args>...>; //the code can't compile in vs2017, why?maybe args... in if constexpr?
                static_assert(!(iguana::has_type<ormpp_key, U>::value&&iguana::has_type<ormpp_auto_key, U>::value), "should only one key");
            }

            auto tp = sort_tuple(std::make_tuple(std::forward<Args>(args)...));
            const size_t arr_size = arr.size();
            for(size_t i=0; i< arr_size; ++i) {
                auto field_name = arr[i];
                bool has_add_field = false;
                for_each0(tp, [&sql, &i, &has_add_field, field_name, type_name_arr,name, this](auto item){
                    if constexpr (std::is_same_v<decltype(item), ormpp_not_null>){
                    if(item.fields.find(field_name.data())==item.fields.end())
                        return;
                    }
                    else{
                    if(item.fields != field_name.data())
                        return;
                    }

                    if constexpr (std::is_same_v<decltype(item), ormpp_not_null>){
                    if(!has_add_field){
                        append(sql, field_name.data(), " ", type_name_arr[i]);
                    }
                    append(sql, " NOT NULL");
                    has_add_field = true;
                }
                    else if constexpr (std::is_same_v<decltype(item), ormpp_key>){
                    if(!has_add_field){
                        append(sql, field_name.data(), " ", type_name_arr[i]);
                    }

                    append(sql, " PRIMARY KEY ");
                    has_add_field = true;
                }
                    else if constexpr (std::is_same_v<decltype(item), ormpp_auto_key>){
                    if(!has_add_field){
                        append(sql, field_name.data(), " ", type_name_arr[i]);
                    }
                    append(sql, " PRIMARY KEY ASC");
                    auto_key_map_[name.data()] = item.fields;
                    has_add_field = true;
                }
					else if constexpr (std::is_same_v<decltype(item), ormpp_unique>) {
						if (!has_add_field) {
							append(sql, field_name.data(), " ", type_name_arr[i]);
						}

						append(sql, ", UNIQUE(", item.fields,")");
						has_add_field = true;
					}
                    else {
                    append(sql, field_name.data(), " ", type_name_arr[i]);
                }
                }, std::make_index_sequence<SIZE>{});

                if(!has_add_field){
                    append(sql, field_name.data(), " ", type_name_arr[i]);
                }

                if(i<arr_size-1)
                    sql+=", ";
            }

            sql += ")";

            return sql;
        }

        struct guard_statment{
            guard_statment(sqlite3_stmt* stmt):stmt_(stmt){}
            sqlite3_stmt* stmt_= nullptr;
            int status_ = 0;
            ~guard_statment(){
                if(stmt_!= nullptr)
                    status_ = sqlite3_finalize(stmt_);

                if(status_)
                    fprintf(stderr, "close statment error code %d\n", status_);
            }
        };

        template<typename T>
        constexpr bool set_param_bind(T&& value, int i){
            using U = std::remove_const_t<std::remove_reference_t<T>>;
            if constexpr(std::is_integral_v<U>&&!iguana::is_int64_v<U>){//double, int64
                return SQLITE_OK == sqlite3_bind_int(stmt_, i, value);
            }
            else if constexpr (iguana::is_int64_v<U>){
                return SQLITE_OK == sqlite3_bind_int64(stmt_, i, value);
            }
            else if constexpr (std::is_floating_point_v<U>){
                return SQLITE_OK == sqlite3_bind_double(stmt_, i, value);
            }
            else if constexpr(std::is_same_v<std::string, U>){
                return SQLITE_OK == sqlite3_bind_text(stmt_, i,  value.data(), (int)value.size(), nullptr);
            }
			else if constexpr(is_char_array_v<U>) {
				return SQLITE_OK == sqlite3_bind_text(stmt_, i, value, sizeof(U), nullptr);
			}
            else {
                std::cout<<"this type has not supported yet"<<std::endl;
                return false;
            }
        }

        template<typename T>
        constexpr void assign(T&& value, int i){
            using U = std::remove_const_t<std::remove_reference_t<T>>;
            if constexpr(std::is_integral_v<U>&&!iguana::is_int64_v<U>){//double, int64
                value = sqlite3_column_int(stmt_, i);
            }
            else if constexpr (iguana::is_int64_v<U>){
                value = sqlite3_column_int64(stmt_, i);
            }
            else if constexpr (std::is_floating_point_v<U>){
                value = sqlite3_column_double(stmt_, i);
            }
            else if constexpr(std::is_same_v<std::string, U>){
                value.reserve(sqlite3_column_bytes(stmt_, i));
                value.assign((const char*)sqlite3_column_text(stmt_, i), (size_t)sqlite3_column_bytes(stmt_, i));
            }
			else if constexpr (is_char_array_v<U>) {
				memcpy(value, sqlite3_column_text(stmt_, i), sizeof(U));
			}
            else {
                std::cout<<"this type has not supported yet"<<std::endl;
            }
        }

        template<typename T, typename... Args>
        constexpr int insert_impl(bool is_update, const std::string& sql, const T& t, Args&&... args) {
            int result = sqlite3_prepare_v2(handle_, sql.data(), (int)sql.size(), &stmt_, nullptr);
            if (result != SQLITE_OK)
                return INT_MIN;

            auto guard = guard_statment(stmt_);

            auto it = auto_key_map_.find(iguana::get_name<T>().data());
            std::string auto_key = (is_update||it==auto_key_map_.end())?"":it->second;
            bool bind_ok = true;
            int index = 0;
            iguana::for_each(t, [&t, &bind_ok, &auto_key, &index, this](auto item, auto i){
                if(!bind_ok)
                    return;

                /*if(!auto_key.empty()&&auto_key==iguana::get_name<T>(decltype(i)::value).data()){
                    return;
                }*/

                bind_ok = set_param_bind(t.*item, index+1);
                index++;
            });

            if(!bind_ok)
                return INT_MIN;

            result = sqlite3_step(stmt_);
            if (result != SQLITE_DONE)
                return INT_MIN;

            return 1;
        }

        template<typename T, typename... Args>
        constexpr int insert_impl(bool is_update, const std::string& sql, const std::vector<T>& v, Args&&... args) {
            int result = sqlite3_prepare_v2(handle_, sql.data(), (int)sql.size(), &stmt_, nullptr);
            if (result != SQLITE_OK)
                return INT_MIN;

            auto guard = guard_statment(stmt_);

            bool b = begin();
            if(!b)
                return INT_MIN;

            auto it = auto_key_map_.find(iguana::get_name<T>().data());
            std::string auto_key = (is_update||it==auto_key_map_.end())?"":it->second;

            for(auto& t : v){
                bool bind_ok = true;
                int index = 0;
                iguana::for_each(t, [&t, &bind_ok, &auto_key, &index, this](auto item, auto i){
                    if(!bind_ok)
                        return;

                    if(!auto_key.empty()&&auto_key==iguana::get_name<T>(decltype(i)::value).data()){
                        return;
                    }

                    bind_ok = set_param_bind(t.*item, index+1);
                    index++;
                });

                if(!bind_ok){
                    rollback();
                    return INT_MIN;
                }

                result = sqlite3_step(stmt_);
                if (result != SQLITE_DONE){
                    rollback();
                    return INT_MIN;
                }

                result = sqlite3_reset(stmt_);
                if (result != SQLITE_OK){
                    rollback();
                    return INT_MIN;
                }
            }

            b = commit();

            return b?(int)v.size():INT_MIN;
        }

        sqlite3* handle_ = nullptr;
        sqlite3_stmt* stmt_ = nullptr;
        std::map<std::string, std::string> auto_key_map_;
//        std::string auto_key_ = "";
    };
}
#endif //ORM_SQLITE_HPP
