#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::string mangle_type(std::string t) {
    t = trim(t);
    std::string out;
    for (char c : t)
        out += (c == '*') ? 'p' : (c == ' ') ? '_' : c;
    std::string clean; bool pu = false;
    for (char c : out) { if (c=='_'&&pu) continue; pu=(c=='_'); clean+=c; }
    while (!clean.empty() && clean.back() == '_') clean.pop_back();
    return clean;
}

static std::string replace_T(const std::string& src, const std::string& repl) {
    return std::regex_replace(src, std::regex("\\bT\\b"), repl);
}

static bool is_ident_char(char c) { return std::isalnum((unsigned char)c)||c=='_'; }

static size_t scan_angle(const std::string& s, size_t pos) {
    if (s[pos]!='<') return std::string::npos;
    int depth=1; ++pos;
    while (pos<s.size()&&depth>0) { if(s[pos]=='<')++depth; else if(s[pos]=='>')--depth; ++pos; }
    return depth==0 ? pos-1 : std::string::npos;
}

static std::string resolve_include(const std::string& name,
                                    const fs::path& cur,
                                    const std::vector<std::string>& paths) {
    fs::path rel = cur.parent_path() / name;
    if (fs::exists(rel)) return rel.lexically_normal().string();
    for (auto& p : paths) {
        fs::path c = fs::path(p) / name;
        if (fs::exists(c)) return c.lexically_normal().string();
    }
    return "";
}

static std::regex make_include_re() {
    return std::regex("\\s*#\\s*include\\s*\"([^\"]+)\".*");
}

static std::pair<std::string, std::set<std::string>>
inline_includes(const std::string& path,
                const std::vector<std::string>& search_paths,
                std::set<std::string>& visited) {
    std::string canon = fs::weakly_canonical(path).string();
    if (visited.count(canon)) return {"", {}};
    visited.insert(canon);

    std::ifstream f(path);
    if (!f) { std::cerr << "Cannot open: " << path << "\n"; std::exit(1); }
    std::string source((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    static std::regex inc_re = make_include_re();
    std::string out;
    std::set<std::string> inlined_paths;

    std::istringstream ss(source);
    std::string line;
    while (std::getline(ss, line)) {
        std::smatch m;
        if (std::regex_match(line, m, inc_re)) {
            std::string inc_name = m[1];
            std::string resolved = resolve_include(inc_name, path, search_paths);
            if (!resolved.empty()) {
                inlined_paths.insert(inc_name);
                auto [sub, sub_inlined] = inline_includes(resolved, search_paths, visited);
                out += sub;
                inlined_paths.insert(sub_inlined.begin(), sub_inlined.end());
            } else {
                out += line + "\n";
            }
        } else {
            out += line + "\n";
        }
    }
    return {out, inlined_paths};
}

struct TemplateDef {
    std::string return_type, func_name, params, body;
    size_t src_start=0, src_end=0;
};

static std::vector<TemplateDef> parse_templates(const std::string& code) {
    std::vector<TemplateDef> res;
    std::regex re(
        "template\\s*<\\s*typename\\s+T\\s*>\\s*"
        "(\\w[\\w\\s\\*]*?)\\s+(\\w+)\\s*\\(([^)]*)\\)\\s*\\{([\\s\\S]*?)\\n\\}"
    );
    for (auto it=std::sregex_iterator(code.begin(),code.end(),re);
         it!=std::sregex_iterator();++it) {
        TemplateDef td;
        td.return_type=trim((*it)[1].str());
        td.func_name=(*it)[2].str();
        td.params=(*it)[3].str();
        td.body=(*it)[4].str();
        td.src_start=(size_t)it->position();
        td.src_end=td.src_start+(size_t)it->length();
        res.push_back(td);
    }
    return res;
}

static std::string instantiate(const TemplateDef& td, const std::string& type) {
    return "static inline " +
           replace_T(td.return_type,type) + " " +
           td.func_name + "_" + mangle_type(type) +
           "(" + replace_T(td.params,type) + ") {" +
           replace_T(td.body,type) + "\n}";
}

static std::set<std::string> collect_used_types(const std::string& src,
                                                  const std::string& name) {
    std::set<std::string> types; size_t i=0;
    while (i<src.size()) {
        if(src[i]=='"'){++i;while(i<src.size()&&src[i]!='"'){if(src[i]=='\\')++i;++i;}++i;continue;}
        if(src[i]=='\''){++i;while(i<src.size()&&src[i]!='\''){if(src[i]=='\\')++i;++i;}++i;continue;}
        if(i+1<src.size()&&src[i]=='/'&&src[i+1]=='/'){while(i<src.size()&&src[i]!='\n')++i;continue;}
        if(i+1<src.size()&&src[i]=='/'&&src[i+1]=='*'){i+=2;while(i+1<src.size()&&!(src[i]=='*'&&src[i+1]=='/'))++i;if(i+1<src.size())i+=2;continue;}
        if(std::isalpha((unsigned char)src[i])||src[i]=='_'){
            size_t s=i; while(i<src.size()&&is_ident_char(src[i]))++i;
            std::string id=src.substr(s,i-s);
            size_t ws=i; while(ws<src.size()&&src[ws]==' ')++ws;
            if(id==name&&ws<src.size()&&src[ws]=='<'){
                size_t ae=scan_angle(src,ws);
                if(ae!=std::string::npos){types.insert(trim(src.substr(ws+1,ae-ws-1)));i=ae+1;continue;}
            }
            continue;
        }
        ++i;
    }
    return types;
}

static std::string rewrite_callsites(const std::string& src,
                                      const std::set<std::string>& known) {
    std::string out; out.reserve(src.size()); size_t i=0;
    while(i<src.size()){
        if(src[i]=='"'){out+=src[i++];while(i<src.size()){if(src[i]=='\\'){out+=src[i++];if(i<src.size())out+=src[i++];continue;}out+=src[i];if(src[i++]=='"')break;}continue;}
        if(src[i]=='\''){out+=src[i++];while(i<src.size()){if(src[i]=='\\'){out+=src[i++];if(i<src.size())out+=src[i++];continue;}out+=src[i];if(src[i++]=='\'')break;}continue;}
        if(i+1<src.size()&&src[i]=='/'&&src[i+1]=='/'){while(i<src.size()&&src[i]!='\n')out+=src[i++];continue;}
        if(i+1<src.size()&&src[i]=='/'&&src[i+1]=='*'){out+=src[i++];out+=src[i++];while(i+1<src.size()&&!(src[i]=='*'&&src[i+1]=='/'))out+=src[i++];if(i+1<src.size()){out+=src[i++];out+=src[i++];}continue;}
        if(std::isalpha((unsigned char)src[i])||src[i]=='_'){
            size_t s=i; while(i<src.size()&&is_ident_char(src[i]))++i;
            std::string id=src.substr(s,i-s);
            size_t ws=i; while(ws<src.size()&&src[ws]==' ')++ws;
            if(known.count(id)&&ws<src.size()&&src[ws]=='<'){
                size_t ae=scan_angle(src,ws);
                if(ae!=std::string::npos){out+=id+"_"+mangle_type(trim(src.substr(ws+1,ae-ws-1)));i=ae+1;continue;}
            }
            out+=id;continue;
        }
        out+=src[i++];
    }
    return out;
}// ^ some beautiful code now that im looking at it

static std::string strip_templates(std::string src, const std::vector<TemplateDef>& tdefs) {
    auto tmp=tdefs;
    std::sort(tmp.begin(),tmp.end(),[](auto&a,auto&b){return a.src_start>b.src_start;});
    for(auto&td:tmp){size_t s=td.src_start;if(s>0&&src[s-1]=='\n')--s;src.erase(s,td.src_end-s);}
    return src;
}


static std::string strip_inlined_includes(const std::string& src,
                                           const std::set<std::string>& inlined_names) {
    static std::regex inc_re = make_include_re();
    std::string out;
    std::istringstream ss(src);
    std::string line;
    while (std::getline(ss, line)) {
        std::smatch m;
        if (std::regex_match(line, m, inc_re)) {
            if (inlined_names.count(std::string(m[1]))) {
                out += "/* ctemplate: inlined " + std::string(m[1]) + " */\n";
                continue;
            }
        }
        out += line + "\n";
    }
    return out;
}

static size_t find_insert_pos(const std::string& src) {
    size_t pos=0;
    for (size_t p=0;p<src.size();) {
        if(src[p]=='#'){size_t q=p;while(q<src.size()&&src[q]!='\n')++q;
            if(src.substr(p,q-p).find("include")!=std::string::npos)pos=q+1;p=q+1;}
        else ++p;
    }
    return pos;
}

static void process(const std::string& input_file,
                    const std::string& output_file,
                    const std::vector<std::string>& search_paths) {

    std::set<std::string> visited;
    auto [expanded, inlined_paths] = inline_includes(input_file, search_paths, visited);

    auto tdefs = parse_templates(expanded);
    std::set<std::string> known_names;
    for (auto& td : tdefs) known_names.insert(td.func_name);

    std::string inst_block;
    if (!tdefs.empty()) {
        inst_block += "\n/* ---- ctemplate instantiations ---- */\n";
        for (auto& td : tdefs) {
            auto used = collect_used_types(expanded, td.func_name);
            if (used.empty()) continue;
            inst_block += "/* template: " + td.func_name + " */\n";
            for (auto& t : used)
                inst_block += instantiate(td, t) + "\n\n";
        }
        inst_block += "/* ---- end instantiations ---- */\n\n";
    }

    std::ifstream orig_f(input_file);
    std::string orig_src((std::istreambuf_iterator<char>(orig_f)),
                          std::istreambuf_iterator<char>());

    std::string no_tmpl_includes = strip_inlined_includes(orig_src, inlined_paths);

    auto orig_tdefs = parse_templates(no_tmpl_includes);    std::string stripped = strip_templates(no_tmpl_includes, orig_tdefs);
    std::string rewritten = rewrite_callsites(stripped, known_names);

    size_t ins = find_insert_pos(rewritten);
    std::string output =
        "/* edit the source .c, not this file */\n" +
        rewritten.substr(0, ins) +
        inst_block +
        rewritten.substr(ins);

    fs::create_directories(fs::path(output_file).parent_path());
    std::ofstream out_f(output_file);
    if (!out_f) { std::exit(1); }
    out_f << output;
    std::cout << "Generated: " << output_file << "\n";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        // i dont think anyone is actually using this? no need to print usage
        return 1;
    }
    std::vector<std::string> search_paths;
    std::string input_file, output_file;
    for (int i=1;i<argc;++i) {
        std::string arg=argv[i];
        if(arg.rfind("-I",0)==0) search_paths.push_back(arg.substr(2));
        else if(input_file.empty()) input_file=arg;
        else output_file=arg;
    }
    if(input_file.empty()||output_file.empty()){return 1;}
    process(input_file, output_file, search_paths);
    return 0;
}
