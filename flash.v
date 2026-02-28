module flash
 #(parameter    boot_file = "rom.mem",
   parameter    pmem_width = 10
 ) (
    input    clk,
    input    [7:0] mmu_a,            // MMU register selection
    output   [7:0] mmu_dout,         // Data to the CPU
    input    [7:0] data_write,       // Data from the CPU
    input    mmu_re,                 // MMU register read enable
    input    mmu_we,                 // MMU register write enable
    input    pmem_ce,
    input    [pmem_width-1:0] pmem_a,
    output   [15:0] pmem_d
 );

    reg  [15:0] bootrom [0:2**pmem_width-1]; // boot FPGA ROM (128KB)
    reg  [15:0] bootinstr;                   // registered RAM output
    reg  [15:0] memvalue;                    // registered RAM output
    reg  [7:0] tmplow;                       // Low byte of app program data
    reg  [pmem_width-1:0] appaddr;           // R/W addr in app ROM for MMU

    initial begin
        $readmemh(boot_file, bootrom);
        appaddr = 0;
        tmplow = 0;
    end

    always @(posedge clk)
    begin
        if (pmem_ce) 
            bootinstr <= bootrom[pmem_a];
        else
            memvalue <=  bootrom[appaddr];
    end

    always @(posedge clk)
    begin
        if (mmu_we & (mmu_a == 8'h0))             // set low RW addr
            appaddr[7:0] <= data_write;
        else if (mmu_we & (mmu_a == 8'h1))        // set high RW addr
            appaddr[pmem_width-1:8] <= data_write;
        else if (mmu_we & (mmu_a == 8'h2))        // set low ROM byte
            tmplow <= data_write;
        else if (mmu_a == 8'h3)                   // set high ROM byte
        begin
            if (mmu_we)
                bootrom[appaddr] <= {data_write,tmplow};
            // increment ROM pointer on read or write of high byte
            if (mmu_we | mmu_re)
                appaddr <= appaddr + 16'h1;
        end
    end

    assign mmu_dout = (mmu_a == 8'h0) ? appaddr[7:0] :
                      (mmu_a == 8'h1) ? appaddr[15:8] :
                      (mmu_a == 8'h2) ? memvalue[7:0] :
                      (mmu_a == 8'h3) ? memvalue[15:8] :
                      8'h55;
    assign pmem_d = bootinstr;

endmodule


