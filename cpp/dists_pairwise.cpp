#include <memory>

#include "../include/args.h"
#include "../include/seq_tools.hpp"
#include "../include/sketch.hpp"
#include <fstream>

// TODO write Google test units


struct cmd_args : public tensor_embed_args, public kmer_args, public seq_args {
    int ksig_len;
    cmd_args(int argc, char* argv[])  : tensor_embed_args(argc, argv),
                                       kmer_args(argc, argv),
                                       seq_args(argc, argv){
       ksig_len = VecTools::int_pow<size_t>(sig_len, kmer_size);
    }
};

int main(int argc, char* argv[]) {
    using namespace SeqSketch;
    using namespace Types;

    cmd_args cmds(argc, argv);


// generate sequences
    SeqSketch::SeqGen gen(cmds);
    Vec<Seq<int>> seqs;
    gen.gen_seqs(seqs);

    // transform to kmer_seqs
    Vec<Seq<int>> kmer_seqs(cmds.num_seqs);
    for (int si = 0; si < cmds.num_seqs; si++) {
        seq2kmer(seqs[si], kmer_seqs[si], cmds.kmer_size, cmds.sig_len);
    }


    // min hash
    MinHashParams MHparams(cmds.embed_dim, cmds.ksig_len);
    Vec2D<int> MHembed(cmds.num_seqs);

    // weighted min hash
    WeightedMinHashParams WMHparams(cmds.embed_dim, cmds.ksig_len, cmds.seq_len * 2);
    Vec2D<int> WMHembed(cmds.num_seqs);


    // init OMP embedding
    OMP_Params omp_params(argc, argv);
    omp_params.sig_len = cmds.ksig_len;
    omp_params.init_rand();
    Vec3D<int> omp_embeddings(cmds.num_seqs);

    // init tensor embed
    TensorParams tensor_params(argc, argv);
    tensor_params.sig_len = cmds.ksig_len;
    tensor_params.rand_init();
    Vec2D<int> tensor_embeddings(cmds.num_seqs);

    // init tensor slide embed
    TensorSlideParams tensor_slide_params(argc, argv);
    tensor_slide_params.num_bins = 64;
    tensor_slide_params.sig_len = cmds.ksig_len;
    tensor_slide_params.embed_dim = cmds.embed_dim/tensor_slide_params.stride;
    tensor_slide_params.rand_init();
    Vec3D<int> tensor_slide_embeddings(cmds.num_seqs);


    for (int si = 0; si < kmer_seqs.size(); si++) {
        const auto &kseq = kmer_seqs[si];
        minhash(kseq, MHembed[si], MHparams);
        weighted_minhash(kseq, WMHembed[si], WMHparams);
        omp_sketch(kseq, omp_embeddings[si], omp_params);
        tensor_sketch(kseq, tensor_embeddings[si], tensor_params);
        tensor_sketch_slide(kseq, tensor_slide_embeddings[si], tensor_slide_params);
    }

    auto dists = new3D<int>(6, cmds.num_seqs, cmds.num_seqs, 0);
    for (int i = 0; i < seqs.size(); i++) {
        for (int j = i + 1; j < seqs.size(); j++) {
            dists[0][i][j] = SeqSketch::edit_distance(seqs[i], seqs[j]);
            dists[1][i][j] = VecTools::hamming_dist(MHembed[i], MHembed[j]);
            dists[2][i][j] = VecTools::hamming_dist(WMHembed[i], WMHembed[j]);
            dists[3][i][j] = VecTools::hamming_dist2D(omp_embeddings[i], omp_embeddings[j]);
            dists[4][i][j] = VecTools::l1_dist(tensor_embeddings[i], tensor_embeddings[j]);
            dists[5][i][j] = VecTools::l1_dist2D_minlen(tensor_slide_embeddings[i] , tensor_slide_embeddings[j]);
        }
    }

    std::ofstream fo;
    fo.open("output.txt");
    for (int i = 0; i < seqs.size(); i++) {
        for (int j = i + 1; j < seqs.size(); j++) {
            fo << dists[0][i][j] << ", " << dists[1][i][j] << ", " << dists[2][i][j] << ", " << dists[3][i][j] << ", " << dists[4][i][j] << ", " << dists[5][i][j] << "\n";
        }
    }
    fo.close();
}
