// Labels addresses from a TSV (rva<TAB>name | name...) made from Address Library 1.6.1170 + CommonLibSSE-NG.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.*;
public class ApplyNames extends GhidraScript {
    @Override
    public void run() throws Exception {
        String path = getScriptArgs().length > 0 ? getScriptArgs()[0] : "names1170.tsv";
        long base = currentProgram.getImageBase().getOffset();
        int n = 0, fn = 0;
        for (String line : Files.readAllLines(Paths.get(path))) {
            String[] p = line.split("\t", 2);
            if (p.length < 2) continue;
            Address a = toAddr(base + Long.parseLong(p[0], 16));
            String name = p[1].split(" \\| ")[0].replaceAll("[^A-Za-z0-9_:~]", "_");
            String plate = p[1];
            try {
                Function f = getFunctionAt(a);
                if (f == null && !p[1].startsWith("VTABLE_") && !p[1].startsWith("RTTI_")) {
                    disassemble(a);
                    f = createFunction(a, null);
                }
                if (f != null) {
                    f.setName(name.replace("::", "__"), SourceType.USER_DEFINED);
                    ++fn;
                } else {
                    createLabel(a, name.replace("::", "__"), true, SourceType.USER_DEFINED);
                }
                setPlateComment(a, "CommonLibSSE-NG: " + plate);
                ++n;
            } catch (Exception e) {
                println("skip " + p[0] + " " + e.getMessage());
            }
        }
        println("labelled " + n + " (functions " + fn + ")");
    }
}
