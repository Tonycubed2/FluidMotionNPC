// Decompiles functions at the given RVAs (args: outFile rva1 rva2 ...) into one text file.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.Function;
import java.io.*;
public class Decomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long base = currentProgram.getImageBase().getOffset();
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            for (int i = 1; i < a.length; ++i) {
                var addr = toAddr(base + Long.parseLong(a[i], 16));
                Function f = getFunctionContaining(addr);
                if (f == null) { disassemble(addr); f = createFunction(addr, null); }
                if (f == null) { w.println("// no function at " + a[i]); continue; }
                var r = d.decompileFunction(f, 120, monitor);
                w.println("// ==== " + f.getName() + " @ +" + Long.toHexString(f.getEntryPoint().getOffset() - base));
                w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// failed: " + r.getErrorMessage());
            }
        }
    }
}
