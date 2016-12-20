#!/usr/bin/env Rscript
library(optparse) #sudo apt install r-cran-optparse 
library(ggplot2) #sudo apt-get install r-cran-ggplot2

option_list <- list(
  make_option(c("--csvfile"), type="character", default="tmp_data.csv", 
              help="dataset file name", metavar="character"),
	make_option(c("--out"), type="character", default="performance_plot.pdf", 
              help="output file name [default= %default]", metavar="character"),
	make_option(c("--title"), type="character", default=NULL, 
              help="add title to the plot", metavar="character"),
	make_option(c("--ltitle"), type="character", default=NULL, 
              help="add title to the legend", metavar="character"),
	make_option(c("--xlabel"), type="character", default="Number of Cores [#]", 
              help="set label of x-axis", metavar="character"),
	make_option(c("--ylabel"), type="character", default="Time [s]", 
              help="set label of y-axis", metavar="character"),
	make_option(c("--ydivider"), type="double", default=1, 
              help="set divider for y-axis", metavar="double")
); 
 
opt_parser = OptionParser(option_list=option_list);
opt = parse_args(opt_parser);

data <- read.csv(opt$csvfile)
data$y_aes <- data$y_aes / opt$ydivider
data$err <- data$err / opt$ydivider

plot <- ggplot (data, aes(x=x_aes, y=y_aes)) +

    geom_errorbar(aes(ymin = y_aes+ (-err), ymax = y_aes+ err, color=type), width = 2) +
    geom_line(aes(color=type), size=0.4) +
    geom_point(aes(color=type, shape=type), size=2) +
    labs(x=opt$xlabel, y=opt$ylabel) +
    scale_color_discrete(name=opt$ltitle) +
    scale_shape_discrete(name=opt$ltitle) +
    theme_bw() 

if (!is.null(opt$title)) {
    plot <- plot + ggtitle(opt$title)    
}

ggsave(opt$out, plot, width=7.2, height=4.6)
