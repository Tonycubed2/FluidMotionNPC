// Args: outFile rva...  Lists every function referencing each address (including through a pointer to it).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.listing.Function;
import java.io.*;
public class Refs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long base = currentProgram.getImageBase().getOffset();
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            for (int i = 1; i < a.length; ++i) {
                var addr = toAddr(base + Long.parseLong(a[i], 16));
                w.println("== refs to +" + a[i]);
                for (Reference r : getReferencesTo(addr)) {
                    Function f = getFunctionContaining(r.getFromAddress());
                    w.println("  from +" + Long.toHexString(r.getFromAddress().getOffset() - base) + " in " +
                        (f == null ? "?" : f.getName() + " @+" + Long.toHexString(f.getEntryPoint().getOffset() - base)));
                }
            }
        }
    }
}
