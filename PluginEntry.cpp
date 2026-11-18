// Ponto de entrada exigido pelo JUCE: cria a instância do processador.
#include "PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PsyProcessor();
}
