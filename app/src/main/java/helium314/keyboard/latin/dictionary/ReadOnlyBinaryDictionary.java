/*
 * Copyright (C) 2013 The Android Open Source Project
 * modified
 * SPDX-License-Identifier: Apache-2.0 AND GPL-3.0-only
 */

package helium314.keyboard.latin.dictionary;

import com.android.inputmethod.latin.BinaryDictionary;

import helium314.keyboard.latin.NgramContext;
import helium314.keyboard.latin.SinhalaLookup;
import helium314.keyboard.latin.SuggestedWords.SuggestedWordInfo;
import helium314.keyboard.latin.common.ComposedData;
import helium314.keyboard.latin.makedict.WordProperty;
import helium314.keyboard.latin.settings.SettingsValuesForSuggestion;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;
import java.util.concurrent.locks.ReentrantReadWriteLock;

/**
 * This class provides binary dictionary reading operations with locking. An instance of this class
 * can be used by multiple threads. Note that different session IDs must be used when multiple
 * threads get suggestions using this class.
 */
public final class ReadOnlyBinaryDictionary extends Dictionary {
    /**
     * A lock for accessing binary dictionary. Only closing binary dictionary is the operation
     * that change the state of dictionary.
     */
    private final ReentrantReadWriteLock mLock = new ReentrantReadWriteLock();

    /** Distinct native traverse session for the Sinhala NFC query. */
    private static final int SINHALA_NORMALIZED_SESSION_OFFSET = 0x40000000;

    private final BinaryDictionary mBinaryDictionary;

    public ReadOnlyBinaryDictionary(final String filename, final long offset, final long length,
            final boolean useFullEditDistance, final Locale locale, final String dictType) {
        super(dictType, locale);
        mBinaryDictionary = new BinaryDictionary(filename, offset, length, useFullEditDistance,
                locale, dictType, false /* isUpdatable */);
    }

    public boolean isValidDictionary() {
        return !mBinaryDictionary.isCorrupted();
    }

    @Override
    public ArrayList<SuggestedWordInfo> getSuggestions(final ComposedData composedData,
            final NgramContext ngramContext, final long proximityInfoHandle,
            final SettingsValuesForSuggestion settingsValuesForSuggestion,
            final int sessionId, final float weightForLocale,
            final float[] inOutWeightOfLangModelVsSpatialModel) {
        if (mLock.readLock().tryLock()) {
            try {
                final ArrayList<SuggestedWordInfo> suggestions = mBinaryDictionary.getSuggestions(
                        composedData, ngramContext, proximityInfoHandle,
                        settingsValuesForSuggestion, sessionId, weightForLocale,
                        inOutWeightOfLangModelVsSpatialModel);
                if (composedData.mIsBatchMode) {
                    return suggestions;
                }
                final String normalizedWord = SinhalaLookup.nfcFormIfDifferent(
                        composedData.mTypedWord);
                if (normalizedWord == null) {
                    return suggestions;
                }
                // The first query already wrote the caller's weight. Keep that value.
                final float[] weightCopy = inOutWeightOfLangModelVsSpatialModel == null
                        ? null
                        : Arrays.copyOf(inOutWeightOfLangModelVsSpatialModel,
                                inOutWeightOfLangModelVsSpatialModel.length);
                final ArrayList<SuggestedWordInfo> normalizedSuggestions =
                        mBinaryDictionary.getSuggestions(
                                SinhalaLookup.exactLookupData(normalizedWord), ngramContext,
                                proximityInfoHandle, settingsValuesForSuggestion,
                                sessionId ^ SINHALA_NORMALIZED_SESSION_OFFSET, weightForLocale,
                                weightCopy);
                return SinhalaLookup.mergeSuggestions(suggestions, normalizedSuggestions);
            } finally {
                mLock.readLock().unlock();
            }
        }
        return null;
    }

    @Override
    public boolean isInDictionary(final String word) {
        if (mLock.readLock().tryLock()) {
            try {
                if (mBinaryDictionary.isInDictionary(word)) {
                    return true;
                }
                final String normalizedWord = SinhalaLookup.nfcFormIfDifferent(word);
                return normalizedWord != null && mBinaryDictionary.isInDictionary(normalizedWord);
            } finally {
                mLock.readLock().unlock();
            }
        }
        return false;
    }

    @Override
    public boolean shouldAutoCommit(final SuggestedWordInfo candidate) {
        if (mLock.readLock().tryLock()) {
            try {
                return mBinaryDictionary.shouldAutoCommit(candidate);
            } finally {
                mLock.readLock().unlock();
            }
        }
        return false;
    }

    @Override
    public int getFrequency(final String word) {
        if (mLock.readLock().tryLock()) {
            try {
                final int frequency = mBinaryDictionary.getFrequency(word);
                if (frequency != NOT_A_PROBABILITY) {
                    return frequency;
                }
                final String normalizedWord = SinhalaLookup.nfcFormIfDifferent(word);
                if (normalizedWord == null) {
                    return NOT_A_PROBABILITY;
                }
                return mBinaryDictionary.getFrequency(normalizedWord);
            } finally {
                mLock.readLock().unlock();
            }
        }
        return NOT_A_PROBABILITY;
    }

    @Override
    public int getMaxFrequencyOfExactMatches(final String word) {
        if (mLock.readLock().tryLock()) {
            try {
                final int frequency = mBinaryDictionary.getMaxFrequencyOfExactMatches(word);
                if (frequency != NOT_A_PROBABILITY) {
                    return frequency;
                }
                final String normalizedWord = SinhalaLookup.nfcFormIfDifferent(word);
                if (normalizedWord == null) {
                    return NOT_A_PROBABILITY;
                }
                return mBinaryDictionary.getMaxFrequencyOfExactMatches(normalizedWord);
            } finally {
                mLock.readLock().unlock();
            }
        }
        return NOT_A_PROBABILITY;
    }

    @Override
    public WordProperty getWordProperty(String word, boolean isBeginningOfSentence) {
        if (mLock.readLock().tryLock()) {
            try {
                final WordProperty property = mBinaryDictionary.getWordProperty(word,
                        isBeginningOfSentence);
                if (property != null) {
                    return property;
                }
                final String normalizedWord = SinhalaLookup.nfcFormIfDifferent(word);
                if (normalizedWord == null) {
                    return null;
                }
                return mBinaryDictionary.getWordProperty(normalizedWord, isBeginningOfSentence);
            } finally {
                mLock.readLock().unlock();
            }
        }
        return null;
    }

    @Override
    public void close() {
        mLock.writeLock().lock();
        try {
            mBinaryDictionary.close();
        } finally {
            mLock.writeLock().unlock();
        }
    }

    public String getHash() {
        return mBinaryDictionary.getHash();
    }
}
