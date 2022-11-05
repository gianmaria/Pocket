#include "pch.h"

string UNIX_epoch_to_local_time(size_t unix_time)
{
    auto sec = std::chrono::seconds{unix_time};

    auto time = date::sys_time<std::chrono::seconds>{sec};

    auto ita_zone = date::make_zoned("Europe/Rome", time);

    std::ostringstream oss;
    oss.imbue(std::locale("it-ch.utf8"));

    date::to_stream(oss,
                    "%A, %d %B %Y - %H:%M:%S %Z",
                    ita_zone);

    return oss.str();

}


int start_server_for_callback()
{
    /*httplib::SSLServer srv(
        "data/cert/localhost.crt",
        "data/cert/localhost.key");*/
    httplib::Server srv;

    if (not srv.is_valid())
    {
        cout << "[ERROR] server is invalid" << endl;
        return 1;
    }

    srv.Get("/pocketapp",
            [](const httplib::Request&,
            httplib::Response& res)
    {
        constexpr auto content = R"(
            <html>
                <head>
                    <title>Pocket App</title>
                </head>
                <body>
                    <h1>Pocket is okega!</h1>
                    <a href="http://localhost:6969/stop">Click to continue</a>
                </body>
            </html>
            )";

        res.set_content(content, "text/html");
    });

    srv.Get("/stop",
            [&srv](const httplib::Request&,
            httplib::Response&)
    {
        srv.stop();
    });

    bool ok = srv.listen("localhost", 6969);

    if (not ok)
    {
        cout << "[ERROR] Cannot start server" << endl;
        return 1;
    }

    return 0;
}

string obtain_request_token(httplib::SSLClient& cli,
                            string_view consumer_key,
                            string_view redirect_uri)
{
    string code;

    auto path = "/v3/oauth/request";

    httplib::Headers req_headers =
    {
        {"X-Accept", "application/json"}
    };

    constexpr auto fmt = R"(
        {{
            "consumer_key" : "{}",
            "redirect_uri" : "{}"
        }})"sv;
    auto req_body = std::format(fmt, consumer_key,
                                redirect_uri);

    auto content_type = "application/json; charset=UTF-8";

    auto resp = cli.Post(path, req_headers, req_body, content_type);

    if (resp)
    {
        if (resp->status == 200)
        {
            using namespace nlohmann::literals;
            auto json = njson::parse(resp->body);

            code = json["code"].get<string>();
        }
        else
        {
            cout << "[ERROR] Request failed with status: " << resp->status << " reason: " << resp->reason << endl;
            cout << "X-Error-Code: " << resp->get_header_value("X-Error-Code") << endl;
            cout << "X-Error: " << resp->get_header_value("X-Error") << endl;
            cout << resp->body << endl;
        }
    }
    else
    {
        auto err = resp.error();
        cout << "HTTP POST error: " << httplib::to_string(err) << endl;
    }

    return code;
}

njson obtain_pocket_access_token(httplib::SSLClient& cli,
                                 string_view consumer_key,
                                 string_view code)
{
    njson res;

    auto path = "/v3/oauth/authorize";

    httplib::Headers req_headers =
    {
        {"X-Accept", "application/json"}
    };

    constexpr auto fmt = R"(
        {{
            "consumer_key" : "{}",
            "code" : "{}"
        }})"sv;

    auto req_body = std::format(fmt,
                                consumer_key,
                                code);

    auto content_type = "application/json; charset=UTF-8";

    auto resp = cli.Post(path, req_headers, req_body, content_type);

    if (resp)
    {
        if (resp->status == 200)
        {
            using namespace nlohmann::literals;
            res = njson::parse(resp->body);
        }
        else
        {
            cout << "[ERROR] Request failed with status: " << resp->status << " reason: " << resp->reason << endl;
            cout << "X-Error-Code: " << resp->get_header_value("X-Error-Code") << endl;
            cout << "X-Error: " << resp->get_header_value("X-Error") << endl;
            cout << resp->body << endl;
        }
    }
    else
    {
        auto err = resp.error();
        cout << "HTTP POST error: " << httplib::to_string(err) << endl;
    }

    return res;
}

bool save_to_file(const string& content,
                  const string& file_path)
{
    std::ofstream ofs(file_path);

    if (not ofs.is_open())
        return false;

    ofs << content;

    return ofs.good() and not ofs.fail() and not ofs.bad();
}

int perform_pocket_oauth2()
{
    httplib::SSLClient cli("getpocket.com");

    if (not cli.is_valid())
    {
        cout << "[ERROR] Invalid client" << endl;
        return 1;
    }

    auto redirect_uri = "http://localhost:6969/pocketapp";
    auto consumer_key = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    auto request_token = obtain_request_token(cli, consumer_key,
                                              redirect_uri);

    if (request_token.empty())
    {
        cout << "[ERROR] could not obtain request token" << endl;
        return 1;
    }

    auto redirect_url = std::format(
        "https://getpocket.com/auth/authorize?"
        "request_token={}&redirect_uri={}",
        request_token, redirect_uri);

    cout << "Click link to authorize\n"
        << redirect_url << endl;

    auto server_res = start_server_for_callback();

    if (server_res != 0)
    {
        return server_res;
    }

    auto pocket_acc_token =
        obtain_pocket_access_token(cli,
                                   consumer_key,
                                   request_token);

    if (pocket_acc_token.empty())
    {
        return 1;
    }

    cout << "[INFO] Got access token for username: "
        << pocket_acc_token["username"] << endl;

    auto file_path = "pocket_access_token.json";

    if (not save_to_file(pocket_acc_token.dump(2),
        file_path))
    {
        cout << "[WARN] Cannot save file " << file_path << " to disk" << endl;
    }

    return 0;
}

njson pocket_retrive(string_view consumer_key,
                     string_view access_token)
{
    njson res;

    httplib::SSLClient cli("getpocket.com");

    if (not cli.is_valid())
    {
        cout << "[ERROR] Invalid client" << endl;
        return res;
    }

    auto path = "/v3/get";

    httplib::Headers req_headers;

    constexpr auto fmt = R"(
        {{
            "consumer_key" : "{}",
            "access_token" : "{}",
            "state" : "all",
            "contentType" : "article",
            "sort" : "newest",
            "detailType" : "simple",
            "count" : "20",
            "offset" : "0"
        }})"sv;

    auto req_body = std::format(fmt, consumer_key,
                                access_token);

    auto content_type = "application/json";

    auto resp = cli.Post(path, req_headers, req_body, content_type);

    if (resp)
    {
        if (resp->status == 200)
        {
            using namespace nlohmann::literals;
            res = njson::parse(resp->body);
        }
        else
        {
            cout << "[ERROR] Request failed with status: " << resp->status << " reason: " << resp->reason << endl;
            cout << "X-Error-Code: " << resp->get_header_value("X-Error-Code") << endl;
            cout << "X-Error: " << resp->get_header_value("X-Error") << endl;
            cout << resp->body << endl;
        }
    }
    else
    {
        auto err = resp.error();
        cout << "HTTP POST error: " << httplib::to_string(err) << endl;
    }

    return res;
}

int main()
{
    try
    {
        date::set_install("data/tzdata2020a");

        auto pocket_access_token_file = "pocket_access_token.json";

        if (not fs::exists(pocket_access_token_file))
        {
            if (perform_pocket_oauth2() != 0)
            {
                return 1;
            }
        }

        const auto json =
            njson::parse(std::ifstream(pocket_access_token_file));

        auto consumer_key = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
        const auto& access_token = json["access_token"].get<string>();

        cout << "[INFO] Found saved info for user: " << json["username"] << endl;

        auto articles = pocket_retrive(consumer_key, access_token);

        /*cout << articles.dump(2) << endl;
        cout << endl << endl << endl;
        cout << articles.flatten().dump(2) << endl;*/

        save_to_file(articles.dump(2), "articles.json");

        const auto& items = articles["list"];

        for (const auto& item : items)
        {
            const auto& given_title = item["given_title"].get<string>();
            const auto& resolved_title = item["resolved_title"].get<string>();
            const auto& title = not given_title.empty() ? given_title : resolved_title;

            auto time_added = std::stoull(item["time_added"].get<string>());
            const auto& desc = item["excerpt"].get<string>();

            const auto& given_url = item["given_url"].get<string>();

            auto msg = std::format("[{}] {}\n{}\n{}",
                                   UNIX_epoch_to_local_time(time_added),
                                   title, desc, given_url);
            cout << msg << endl << endl;
        }

        return 0;
    }
    catch (const std::exception& e)
    {
        cout << "[EXCEPTION] " << e.what() << endl;
    }

    return 1;
}
