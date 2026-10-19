// Pre-script: turn off the slowest analyzers so a 37 MB exe finishes on 2 CPUs.
import ghidra.app.script.GhidraScript;
import java.util.Map;
public class FastOptions extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] off = { "Decompiler Parameter ID", "Decompiler Switch Analysis", "Aggressive Instruction Finder",
            "Stack", "Windows x86 Thread Environment Block (TEB) Analyzer", "Function ID", "Library Identification",
            "Embedded Media", "Non-Returning Functions - Discovered", "Variadic Function Signature Override",
            "Condense Filler Bytes", "Create Address Tables", "Apply Data Archives", "DWARF", "PDB MSDIA", "PDB Universal" };
        Map<String, String> opts = getCurrentAnalysisOptionsAndValues(currentProgram);
        for (String o : off) {
            if (opts.containsKey(o)) { setAnalysisOption(currentProgram, o, "false"); println("off: " + o); }
        }
    }
}
