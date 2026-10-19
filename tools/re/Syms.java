// Args: outFile substring... Lists symbols whose name contains a substring.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import java.io.*;
public class Syms extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long base = currentProgram.getImageBase().getOffset();
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
            while (it.hasNext()) {
                Symbol s = it.next();
                String n = s.getName(true);
                for (int i = 1; i < a.length; ++i) {
                    if (n.contains(a[i])) { w.println("+" + Long.toHexString(s.getAddress().getOffset() - base) + "  " + n); break; }
                }
            }
        }
    }
}
