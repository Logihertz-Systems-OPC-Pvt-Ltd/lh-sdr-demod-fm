// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        src/exporters.cpp
// Role:          ANSI Table, Canonical JSON, and OASIS SARIF v2.1.0 Implementation
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#include "exporters.hpp"
#include "sdr_auditor.hpp"
#include "json_parser.hpp"

#include <sstream>
#include <iomanip>

namespace logihertz {

std::string Exporters::format_table(const AuditReport& report) {
    std::ostringstream ss;

    ss << "\n================================================================================\n";
    ss << "   Logihertz SDR Receiver Posture Auditor (lh-sdr-fm)\n";
    ss << "   Policy Manifest: " << (report.manifest_path.empty() ? "In-Memory / Direct Input" : report.manifest_path) << "\n";
    ss << "   Posture Score:   " << report.score << "/100   [" << report.status << "]\n";
    ss << "   Summary:         " << report.passed_rules << " passed, " << report.findings.size() << " violations "
       << "(CRIT: " << report.critical_count << ", HIGH: " << report.high_count
       << ", MED: " << report.medium_count << ", LOW: " << report.low_count << ")\n";
    ss << "================================================================================\n\n";

    if (report.findings.empty()) {
        ss << " [OK] All 10 SDR receiver posture controls are satisfied.\n";
        ss << "      Receiver configuration conforms to production-grade sovereign standards.\n\n";
        return ss.str();
    }

    ss << "+----------------+----------+--------+----------+--------------------------------------+\n";
    ss << "| Rule ID        | Severity | Weight | CWE      | Rule Title                           |\n";
    ss << "+----------------+----------+--------+----------+--------------------------------------+\n";

    for (const auto& f : report.findings) {
        std::string short_name = (f.rule_name.size() <= 36) ? f.rule_name : (f.rule_name.substr(0, 33) + "...");
        ss << "| " << std::left << std::setw(14) << f.rule_id
           << " | " << std::setw(8) << f.severity
           << " | " << std::setw(6) << f.weight
           << " | " << std::setw(8) << f.cwe
           << " | " << std::setw(36) << short_name << " |\n";

        std::string msg_sub = (f.message.size() <= 66) ? f.message : (f.message.substr(0, 63) + "...");
        std::string rem_sub = (f.remediation.size() <= 66) ? f.remediation : (f.remediation.substr(0, 63) + "...");
        ss << "| Detail:        " << std::setw(66) << msg_sub << " |\n";
        ss << "| Remediation:   " << std::setw(66) << rem_sub << " |\n";
        ss << "+----------------+----------+--------+----------+--------------------------------------+\n";
    }

    ss << "\n";
    return ss.str();
}

std::string Exporters::format_json(const AuditReport& report) {
    JsonValue root(JsonType::Object);
    root.obj_val.emplace_back("manifest_path", JsonValue(report.manifest_path));
    root.obj_val.emplace_back("score", JsonValue(static_cast<double>(report.score)));
    root.obj_val.emplace_back("status", JsonValue(report.status));
    root.obj_val.emplace_back("total_rules", JsonValue(static_cast<double>(report.total_rules)));
    root.obj_val.emplace_back("passed_rules", JsonValue(static_cast<double>(report.passed_rules)));
    root.obj_val.emplace_back("violations_count", JsonValue(static_cast<double>(report.findings.size())));
    root.obj_val.emplace_back("critical_count", JsonValue(static_cast<double>(report.critical_count)));
    root.obj_val.emplace_back("high_count", JsonValue(static_cast<double>(report.high_count)));
    root.obj_val.emplace_back("medium_count", JsonValue(static_cast<double>(report.medium_count)));
    root.obj_val.emplace_back("low_count", JsonValue(static_cast<double>(report.low_count)));

    JsonValue findings_arr(JsonType::Array);
    for (const auto& f : report.findings) {
        JsonValue item(JsonType::Object);
        item.obj_val.emplace_back("rule_id", JsonValue(f.rule_id));
        item.obj_val.emplace_back("rule_name", JsonValue(f.rule_name));
        item.obj_val.emplace_back("severity", JsonValue(f.severity));
        item.obj_val.emplace_back("weight", JsonValue(static_cast<double>(f.weight)));
        item.obj_val.emplace_back("cwe", JsonValue(f.cwe));
        item.obj_val.emplace_back("message", JsonValue(f.message));
        item.obj_val.emplace_back("remediation", JsonValue(f.remediation));
        findings_arr.arr_val.push_back(std::move(item));
    }
    root.obj_val.emplace_back("findings", std::move(findings_arr));

    return root.to_pretty_string();
}

std::string Exporters::format_sarif(const AuditReport& report) {
    JsonValue root(JsonType::Object);
    root.obj_val.emplace_back(
        "$schema",
        JsonValue("https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json")
    );
    root.obj_val.emplace_back("version", JsonValue("2.1.0"));

    JsonValue runs_arr(JsonType::Array);
    JsonValue run_obj(JsonType::Object);

    // Driver Tool
    JsonValue tool_obj(JsonType::Object);
    JsonValue driver_obj(JsonType::Object);
    driver_obj.obj_val.emplace_back("name", JsonValue("lh-sdr-fm"));
    driver_obj.obj_val.emplace_back("version", JsonValue("0.1.0"));
    driver_obj.obj_val.emplace_back("informationUri", JsonValue("https://logihertz.com"));

    // Rules
    JsonValue rules_arr(JsonType::Array);
    for (const auto& r : SdrAuditor::get_rules()) {
        JsonValue r_obj(JsonType::Object);
        r_obj.obj_val.emplace_back("id", JsonValue(r.id));
        r_obj.obj_val.emplace_back("name", JsonValue(r.name));

        JsonValue full_desc(JsonType::Object);
        full_desc.obj_val.emplace_back("text", JsonValue(r.description));
        r_obj.obj_val.emplace_back("fullDescription", std::move(full_desc));

        JsonValue help_obj(JsonType::Object);
        help_obj.obj_val.emplace_back("text", JsonValue(r.remediation));
        r_obj.obj_val.emplace_back("help", std::move(help_obj));

        JsonValue prop_obj(JsonType::Object);
        prop_obj.obj_val.emplace_back("cwe", JsonValue(r.cwe));
        prop_obj.obj_val.emplace_back("severity", JsonValue(severity_to_string(r.severity)));
        prop_obj.obj_val.emplace_back("weight", JsonValue(static_cast<double>(r.weight)));
        r_obj.obj_val.emplace_back("properties", std::move(prop_obj));

        rules_arr.arr_val.push_back(std::move(r_obj));
    }
    driver_obj.obj_val.emplace_back("rules", std::move(rules_arr));
    tool_obj.obj_val.emplace_back("driver", std::move(driver_obj));
    run_obj.obj_val.emplace_back("tool", std::move(tool_obj));

    // Results
    JsonValue results_arr(JsonType::Array);
    for (const auto& f : report.findings) {
        JsonValue res_obj(JsonType::Object);
        res_obj.obj_val.emplace_back("ruleId", JsonValue(f.rule_id));

        std::string level = "error";
        if (f.severity == "MEDIUM") level = "warning";
        else if (f.severity == "LOW") level = "note";
        res_obj.obj_val.emplace_back("level", JsonValue(level));

        JsonValue msg_obj(JsonType::Object);
        msg_obj.obj_val.emplace_back("text", JsonValue(f.message));
        res_obj.obj_val.emplace_back("message", std::move(msg_obj));

        JsonValue loc_arr(JsonType::Array);
        JsonValue loc_obj(JsonType::Object);
        JsonValue phys_obj(JsonType::Object);
        JsonValue art_obj(JsonType::Object);
        art_obj.obj_val.emplace_back(
            "uri",
            JsonValue(report.manifest_path.empty() ? "manifest.json" : report.manifest_path)
        );
        phys_obj.obj_val.emplace_back("artifactLocation", std::move(art_obj));
        loc_obj.obj_val.emplace_back("physicalLocation", std::move(phys_obj));
        loc_arr.arr_val.push_back(std::move(loc_obj));
        res_obj.obj_val.emplace_back("locations", std::move(loc_arr));

        results_arr.arr_val.push_back(std::move(res_obj));
    }
    run_obj.obj_val.emplace_back("results", std::move(results_arr));
    runs_arr.arr_val.push_back(std::move(run_obj));
    root.obj_val.emplace_back("runs", std::move(runs_arr));

    return root.to_pretty_string();
}

} // namespace logihertz
